import socket
import os
import logging
import json
import urllib3
import threading
from time import time
from enum import Enum
from uuid import uuid1
from setproctitle import setproctitle
import concurrent.futures
from utils import load_dotenv_config, log_config, other_instance_running, text_to_file

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


def download_file(item):
    try:
        # open url
        dest = item.get('destination')
        app_log.info(f"will now download {item.get('url')} to {dest}")

        http = urllib3.PoolManager()
        r = http.request('GET', item.get('url'), preload_content=False)

        # Try to get file total length from the headers, so we can report % of download.
        total_length = get_content_length(r)
        app_log.debug(f"url: {item.get('url')} - total_length: {total_length}")

        got_cnt = 0
        with open(dest, 'wb') as out:  # open local path

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
        app_log.debug(f"download of {dest} finished")

    except Exception as ex:
        app_log.warning(f"failed to download {dest} - {str(ex)}")

    update_status_json(item, StatusOp.REMOVE)    # remove item from status


def download_floppy(item):
    download_file(item)


def create_socket(app_log):
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
        app_log.info(f'Success, got socket: {taskq_sock_path}')
        return sckt
    except Exception as e:
        app_log.warning(f'exception on bind: {str(e)}')
        return False


if __name__ == "__main__":
    load_dotenv_config()

    setproctitle("ce_taskq")      # set process title

    log_config()
    app_log = logging.getLogger()

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
    sock = create_socket(app_log)

    if not sock:
        app_log.error("Cannot run without socket! Terminating.")
        exit(1)

    app_log.info(f"Entering main loop, waiting for messages via: {os.getenv('TASKQ_SOCK_PATH')}")

    # This receiving main loop with receive messages via UNIX domain sockets and submit them to thread pool executor.
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as executor:
        while True:
            try:
                data, address = sock.recvfrom(1024)                 # receive message
                message = json.loads(data)                          # convert from json string to dictionary
                app_log.debug(f'received message: {message}')

                action = message.get('action')                      # try to fetch 'action' value

                if not action:                                      # no action defined? we don't know what to do
                    app_log.warning('Ignored message with empty action.')
                    continue

                # get function for this action (function should have the name as action) and submit it to executor
                fun_for_action = locals().get(action)

                if 'id' not in message:                             # if our message doesn't have id, generate on
                    message['id'] = str(uuid1())

                update_status_json(message, StatusOp.ADD)           # add item to status
                executor.submit(fun_for_action, message)            # submit message to thread executor
            except KeyboardInterrupt:
                app_log.error("Got keyboard interrupt, terminating.")
                break
            except Exception as ex:
                app_log.warning(f"got exception: {str(ex)}")

    # exited main loop and now terminating
    should_run = False
