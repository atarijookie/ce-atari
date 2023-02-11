import socket
import os
import json
import urllib3
import threading
from datetime import datetime
from time import time
from enum import Enum
from uuid import uuid4
from setproctitle import setproctitle
from floppy_image_lists import update_floppy_image_lists, after_floppy_image_list_downloaded  # these imports are needed
import concurrent.futures
from utils import load_dotenv_config, log_config, other_instance_running, text_to_file, unlink_without_fail, \
    setting_load_one, system_custom
import ntplib
from time import ctime
from loguru import logger as app_log

should_run = True
lock = threading.Lock()
status_dict = {}
last_save = 0


# class syntax
class StatusOp(Enum):
    ADD = 1
    UPDATE = 2
    REMOVE = 3


def get_content_length(request):
    # Try to get file total length from the headers, so we can report % of download.
    # If this value is not provided, use '1' instead, and the progress will be in bytes downloaded instead of %.
    total_length = request.headers.get('content-length', 1)

    if isinstance(total_length, str):           # string to int
        total_length = int(total_length)

    if total_length in [0, None]:               # replace 0 and None with 1
        total_length = 1

    return total_length


def store_status_to_file(force=False):
    """ Save status to file from time to time, or when forced. """
    global last_save
    now = time()
    diff = now - last_save  # how much time passed since last update?

    if force or diff > 0.45:                    # last save was some time ago? let's save now
        last_save = now
        status_str = json.dumps(status_dict)    # dict to json string
        status_path = os.getenv('TASKQ_STATUS_PATH')
        text_to_file(status_str, status_path)   # string to file


def update_status_json(item, operation: StatusOp):
    with lock:                                              # acquire lock
        act = item['action']
        id_ = item['id']

        if not status_dict.get(act):                        # this action not in dict yet? add it
            status_dict[act] = {}

        if operation in [StatusOp.ADD, StatusOp.UPDATE]:    # on ADD or UPDATE - store it under id
            status_dict[act][id_] = item

        if operation == StatusOp.REMOVE:                    # on REMOVE - remove it
            status_dict[act].pop(id_, None)

        force = operation in [StatusOp.ADD, StatusOp.REMOVE]    # force saving on item added or removed
        store_status_to_file(force)      # store it all to file


@app_log.catch
def download_file(item):
    try:
        # open url
        dest = item.get('destination')              # the final filename after the download
        dest_during_download = dest + ".dwnld"      # temporary filename during the download

        app_log.info(f"will now download {item.get('url')} to {dest}")

        http = urllib3.PoolManager()
        r = http.request('GET', item.get('url'), preload_content=False)

        # Try to get file total length from the headers, so we can report % of download.
        total_length = get_content_length(r)
        app_log.debug(f"url: {item.get('url')} - total_length: {total_length}")

        got_cnt = 0
        with open(dest_during_download, 'wb') as out:   # open local path - temporary filename during download

            while True:
                data_ = r.read(16*1024)

                if not data_:           # no more data? we're done
                    break

                got_cnt += len(data_)

                if total_length > 1:    # got probably valid total length? calc %
                    item['progress'] = int((got_cnt / total_length) * 100)
                else:                   # don't have valid total length, then just store the size we've stored
                    item['progress'] = got_cnt

                update_status_json(item, StatusOp.UPDATE)       # remove item from status

                out.write(data_)        # write data to file

        r.release_conn()

        unlink_without_fail(dest)                       # delete if something with the final filename exist
        os.rename(dest_during_download, dest)           # rename from something.dwnld to something
        app_log.info(f"download of {dest} finished with success!")

    except Exception as ex:
        app_log.warning(f"failed to download {dest} - {str(ex)}")

    update_status_json(item, StatusOp.REMOVE)    # remove item from status

    # if some action should be triggered after this action, do it now
    after_action = item.get('after_action')

    if after_action:
        # get function for this action (function should have the name as action) and submit it to executor
        fun_for_action = globals().get(after_action)

        if not fun_for_action:      # function not found? fail here
            app_log.warning(f'Could not find handler function for after_action {after_action}! after_action ignored.')
            return

        app_log.info(f'Now executing after_action: {after_action} on item: {item}')
        fun_for_action(item)


def download_floppy(item):
    """ action handler for action 'download_floppy' """
    download_file(item)


def download_list(item):
    """ action handler for action 'download_list' """
    download_file(item)


def get_tz_offset():
    tz = setting_load_one('TIME_UTC_OFFSET', 0)
    tz_in = tz

    try:
        tz = float(tz)
    except Exception as ex:
        app_log.warning(f"failed to convert '{tz}' to float: {str(ex)}")
        tz = 0

    tz = int(tz * 100)
    tz_str = f"{'-' if tz < 0 else '+'}{tz:04}"
    app_log.debug(f"converted '{tz_in}' to '{tz_str}'")
    return tz_str


@app_log.catch
def update_time_from_ntp(item):
    # get ntp server IP from settings
    ntp_server = setting_load_one('TIME_NTP_SERVER', 'europe.pool.ntp.org')
    app_log.info(f"will fetch time from NTP server {ntp_server}")

    date_str = time_str = None
    good = False
    try:
        c = ntplib.NTPClient()
        response = c.request(ntp_server, version=3)
        dt_object = datetime.utcfromtimestamp(response.tx_time)
        date_str = dt_object.strftime('%Y-%m-%d')
        time_str = dt_object.strftime('%H:%M:%S')
        good = True
    except Exception as ex:
        app_log.warning(f"failed to get time from NTP server {ntp_server}: {str(ex)}")

    if not good:            # if failed to fetch date from NTP server, quit
        return

    tz_str = get_tz_offset()

    # log result and set datetime
    app_log.info(f"time from NTP server: {date_str} {time_str} {tz_str}")

    # set date time
    cmd_set_datetime = f"date +'%Y-%m-%d %H:%M:%S %z' -s '{date_str} {time_str} {tz_str}'"
    system_custom(cmd_set_datetime, to_log=True, shell=True)


def create_socket():
    taskq_sock_path = os.getenv('TASKQ_SOCK_PATH')

    try:
        os.unlink(taskq_sock_path)
    except Exception as ex:
        if os.path.exists(taskq_sock_path):
            app_log.warning(f"failed to unlink sock path: {taskq_sock_path} : {str(ex)}")
            raise

    sckt = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)

    try:
        sckt.bind(taskq_sock_path)
        sckt.settimeout(1.0)
        app_log.info(f'Success, got socket: {taskq_sock_path}')
        return sckt
    except Exception as e:
        app_log.warning(f'exception on bind: {str(e)}')
        return False


def add_run_once_tasks(xecutor):
    """ CE and its components need these things to run at least once since power-on of the device. """

    msg = {'action': 'update_time_from_ntp'}
    submit_task_to_executor(xecutor, msg)

    msg = {'action': 'update_floppy_image_lists'}
    submit_task_to_executor(xecutor, msg)


def submit_task_to_executor(xecutor, msg):
    """ Submit one message to executor with all the steps that are needed to be done. """
    action = msg.get('action')  # try to fetch 'action' value

    if not action:  # no action defined? we don't know what to do
        app_log.warning('Ignored message with empty action.')
        return

    # get function for this action (function should have the name as action) and submit it to executor
    fun_for_action = globals().get(action)

    if not fun_for_action:
        app_log.warning(f'Could not find handler function for action {action}! Message ignored.')
        return

    if 'id' not in msg:  # if our message doesn't have id, generate on
        msg['id'] = str(uuid4())

    app_log.debug(f'submitting message to executor: {msg}')
    update_status_json(msg, StatusOp.ADD)   # add item to status
    xecutor.submit(fun_for_action, msg)     # submit message to thread executor


if __name__ == "__main__":
    load_dotenv_config()

    setproctitle("ce_taskq")      # set process title

    log_config()
    #app_log = logging.getLogger()

    # check if running as root, fail and quit if not
    if os.geteuid() != 0:           # If not root user, fail
        app_log.info("You must run this app as root.")
        exit(1)

    # check if other instance is running, quit if it is
    if other_instance_running():
        app_log.info("Other instance is running, this instance won't run!")
        exit(1)

    store_status_to_file()          # store initial state to file

    # try to create socket
    sock = create_socket()

    if not sock:
        app_log.error("Cannot run without socket! Terminating.")
        exit(1)

    app_log.info(f"Entering main loop, waiting for messages via: {os.getenv('TASKQ_SOCK_PATH')}")

    # This receiving main loop with receive messages via UNIX domain sockets and submit them to thread pool executor.
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as executor:
        add_run_once_tasks(executor)                                # add all run-once tasks

        while True:
            try:
                data, address = sock.recvfrom(1024)                 # receive message
                message = json.loads(data)                          # convert from json string to dictionary
                app_log.debug(f'received message: {message}')

                submit_task_to_executor(executor, message)          # ship it!
            except socket.timeout:          # when socket fails to receive data
                pass
            except KeyboardInterrupt:
                app_log.error("Got keyboard interrupt, terminating.")
                break
            except Exception as ex:
                app_log.warning(f"got exception: {str(ex)}")

    # exited main loop and now terminating
    should_run = False
