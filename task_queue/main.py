import socket
import sys
import os
import base64
import queue
import logging
import json
import urllib3
import threading
from setproctitle import setproctitle
import concurrent.futures
from utils import load_dotenv_config, log_config, other_instance_running

should_run = True
task_queue = queue.Queue()      # queue that holds things to download
thr_worker = None

def download_file(item):
    try:
        # open url
        dest = item.get('destination')
        app_log.info(f"will now download {item.get('url')} to {dest}")

        http = urllib3.PoolManager()
        r = http.request('GET', item.get('url'), preload_content=False)

        with open(dest, 'wb') as out:  # open local path
            while True:
                data = r.read(4096)

                if not data:
                    break

                out.write(data)  # write data to file

        r.release_conn()
        app_log.debug(f"download of {dest} finished")

    except Exception as ex:
        app_log.warning(f"failed to download {dest} - {str(ex)}")


def taskq_worker():
    """ download any / all selected images to local storage """
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as executor:
        while should_run:
            got_item = False

            try:
                item = task_queue.get(timeout=0.3)              # get one item to process
                got_item = True

                app_log.info(f"worker got item: {item}")

                if item.get('action') == 'download_floppy':
                    executor.submit(download_file, item)
            except queue.Empty:
                pass
            except Exception as ex:         # we're expecting exception on no item to download
                app_log.warning(f"taskq_worker exception: {str(ex)}")

            if got_item:                    # if managed to get item, mark it as done
                task_queue.task_done()


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

    # try to create socket
    sock = create_socket(app_log)

    if not sock:
        app_log.error("Cannot run without socket! Terminating.")
        exit(1)

    thr_worker = threading.Thread(target=taskq_worker)
    thr_worker.start()

    app_log.info(f"Entering main loop, waiting for messages via: {os.getenv('TASKQ_SOCK_PATH')}")

    # this receiving main loop with receive messages via UNIX domain sockets and put them in the task queue,
    # from where the worker will fetch it and process it
    while True:
        try:
            data, address = sock.recvfrom(1024)
            message = json.loads(data)
            app_log.debug(f'received message: {message}')
            task_queue.put(message)
        except KeyboardInterrupt:
            app_log.error("Got keyboard interrupt, terminating.")
            break
        except Exception as ex:
            app_log.warning(f"got exception: {str(ex)}")

    # exited main loop and now terminating
    should_run = False
