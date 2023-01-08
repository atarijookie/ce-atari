import os
import pyinotify
import asyncio
import socket
import json
from loguru import logger as app_log
import threading, queue
import traceback
from datetime import datetime
from setproctitle import setproctitle
from wrapt_timeout_decorator import timeout
from dotenv import load_dotenv


def load_dotenv_config():
    """ Try to load the dotenv configuration file.
    First try to see if there's an override path for this config file specified in the env variables.
    Then try the normal installation path for dotenv on ce: /ce/services/.env
    If that fails, try to find and use local dotenv file used during development - .env in your local dir
    """

    # First try to see if there's an override path for this config file specified in the env variables.
    path = os.environ.get('CE_DOTENV_PATH')

    if path and os.path.exists(path):       # path in env found and it really exists, use it
        load_dotenv(dotenv_path=path)
        return

    # Then try the normal installation path for dotenv on ce: /ce/services/.env
    ce_dot_env_file = '/ce/services/.env'
    if os.path.exists(ce_dot_env_file):
        load_dotenv(dotenv_path=ce_dot_env_file)
        return

    # If that fails, try to find and use local dotenv file used during development - .env in your local dir
    load_dotenv()


load_dotenv_config()                        # load dotenv before our files


from shared import log_config, DEV_DISK_DIR, \
    get_symlink_path_for_letter, setting_get_bool, unlink_everything_translated, \
    unlink_everything_raw, settings_load, show_symlinked_dirs, \
    symlink_if_needed, other_instance_running, unlink_without_fail, FILE_ROOT_DEV, \
    load_one_setting, get_drives_bitno_from_settings, copy_and_symlink_config_dir
from mount_usb_trans import get_usb_devices, find_and_mount_translated
from mount_usb_raw import find_and_mount_raw
from mount_hdd_image import mount_hdd_image
from mount_on_cmd import mount_on_command, unmount_zip_file_if_source_not_exists
from mount_shared import mount_shared
from mount_user import mount_user_custom_folders


task_queue = queue.Queue()


def worker():
    lock = threading.Lock()

    while True:
        func_and_args = task_queue.get()
        funct = func_and_args['function']
        args = func_and_args['args']
        lock.acquire()
        app_log.info(f'')
        app_log.info(f'EXECUTING: {funct.__name__}()')
        start = datetime.now()

        try:
            if not args:        # no args supplied? call function without arguments
                funct()
            else:               # arguments provided? use them
                funct(args)
        except TimeoutError:
            app_log.warning(f'The function {funct.__name__} was terminated with TimeoutError')
        except Exception as ex:
            app_log.warning(f'The function {funct.__name__} has crashed: {type(ex).__name__} - {str(ex)}')
            tb = traceback.format_exc()
            app_log.warning(tb)

        duration = (datetime.now() - start).total_seconds()
        app_log.info(f'TASK: {funct.__name__}() TOOK {duration:.1f} s')

        lock.release()
        task_queue.task_done()


def create_socket():
    mounter_sock_path = os.getenv('MOUNT_SOCK_PATH')

    try:
        os.unlink(mounter_sock_path)
    except Exception as ex:
        if os.path.exists(mounter_sock_path):
            app_log.warning(f"failed to unlink sock path: {mounter_sock_path} : {str(ex)}")
            raise

    sckt = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)

    try:
        sckt.bind(mounter_sock_path)
        sckt.settimeout(1.0)
        app_log.info(f'Success, got socket: {mounter_sock_path}')
        return sckt
    except Exception as e:
        app_log.warning(f'exception on bind: {str(e)}')
        return False


def sock_receiver():
    """ This loop receives mount commands from socket, puts them in queue and lets them be handled by worker """
    app_log.info(f"Entering sock receiving loop, waiting for messages via: {os.getenv('MOUNT_SOCK_PATH')}")

    while True:
        try:
            data, address = sock.recvfrom(1024)                 # receive message
            message = json.loads(data)                          # convert from json string to dictionary
            app_log.debug(f'received message: {message}')

            task_queue.put({'function': mount_on_command, 'args': message})
        except socket.timeout:          # when socket fails to receive data
            pass
        except KeyboardInterrupt:
            app_log.error("Got keyboard interrupt, terminating.")
            break
        except Exception as ex:
            app_log.warning(f"got exception: {str(ex)}")


def handle_read_callback(ntfr):
    app_log.info(f'monitored folder changed...')

    for path_, handler in watched_paths.items():    # go through the watched paths
        if event_source.get(path_):                 # if this one did change
            event_source[path_] = False             # clear this flag
            app_log.info(f'adding handler to task queue: {handler.__name__}()')
            task_queue.put({'function': handler, 'args': None})   # call the handler


def reload_settings_mount_everything():
    """ this handler gets called when settings change, reload settings and try mounting
    if settings for mounts changed """

    changed_letters, changed_ids = None, None

    try:
        changed_letters, changed_ids = settings_load()
    except Exception as ex:
        app_log.error(f'settings_load excepsion: {str(ex)}')

    if changed_letters:             # if drive letters changed, unlink everything translated
        unlink_everything_translated()

    if changed_ids:                 # if ACSI ID types changed, unlink linked raw devices
        unlink_everything_raw()

    mount_user_custom_folders()     # mount user custom folders if they changed
    copy_and_symlink_config_dir()   # possibly symlink config drive
    mount_shared()                  # either remount shared drive or just symlink it
    find_and_mount_devices(True)    # mount and/or symlink USB devices
    symlink_download_storage()      # symlink download storage dir
    show_symlinked_dirs()           # show the mounts after possible remounts


@timeout(10)
def find_and_mount_devices(dont_show_symlinked_dirs=False):
    """ look for USB devices, find those which are not mounted yet, find a mount point for them, mount them """
    mount_raw_not_trans = setting_get_bool('MOUNT_RAW_NOT_TRANS')
    app_log.info(f"MOUNT mode: {'RAW' if mount_raw_not_trans else 'TRANS'}")

    root_devs, part_devs = get_usb_devices()            # get attached USB devices
    app_log.info(f'devices: {root_devs}')

    if mount_raw_not_trans:         # for raw mount, check if symlinked
        find_and_mount_raw(root_devs)
    else:                           # for translated mounts
        find_and_mount_translated(root_devs, part_devs)

    mount_hdd_image()                           # also try to mount HDD image
    unmount_zip_file_if_source_not_exists()     # unmount ZIP file if it disappeared

    if not dont_show_symlinked_dirs:
        show_symlinked_dirs()       # show the mounts after possible remounts


# following two will help us to determine who caused the event and what function should handle it
event_source = {}
watched_paths = {os.getenv('SETTINGS_DIR'): reload_settings_mount_everything,
                 DEV_DISK_DIR: find_and_mount_devices}


def my_process_event(event):
    """ this event processor is called by pynotify when it processes an event, and we look here into the
    path which caused this event and then mark that handler for this path should be executed
    """

    # app_log.info(f"my_process: {event.__dict__}")

    for path_ in watched_paths.keys():      # go through the watched paths
        if event.path == path_:             # if this watched patch caused this event
            event_source[path_] = True      # mark this event source


def symlink_download_storage():
    """
    Find out where the download storage should point, based on our settings, and create symlink to expected path.
    This makes finding the download storage path easier for any other service (e.g. floppy downloader tool),
    so the search for the download storage doesn't have to be implemented in multiple tools.
    """
    download_storage_type = load_one_setting('DOWNLOAD_STORAGE_TYPE')

    try:
        download_storage_type = int(download_storage_type)
    except TypeError:  # we're expecting this on no text from file
        download_storage_type = 0
    except Exception as ex:
        app_log.warning(f'symlink_download_storage: failed to convert {download_storage_type} to int: {type(ex).__name__} - {str(ex)}')

    # get letters from config, convert them to bit numbers
    first, shared, _ = get_drives_bitno_from_settings()

    drive_letter = 'C'
    if download_storage_type == 0:      # USB? use first letter
        drive_letter = chr(65 + first)
    elif download_storage_type == 1:    # SHARED? use shared letter
        drive_letter = chr(65 + shared)
    elif download_storage_type == 2:    # CUSTOM? get first custom letter
        from mount_user import get_user_custom_mounts_letters
        custom_letters = get_user_custom_mounts_letters()       # get all the custom letters
        if len(custom_letters) > 0:                     # some custom letters are present?
            drive_letter = custom_letters[0]            # use 0th custom letter

    # construct path where the drive letter should be mounted
    download_storage = get_symlink_path_for_letter(drive_letter)

    if not os.path.exists(download_storage):        # this drive doesn't exists? use /tmp then
        download_storage = '/tmp'

    app_log.info(f"symlink_download_storage: {download_storage} -> {os.getenv('DOWNLOAD_STORAGE_DIR')}")
    symlink_if_needed(download_storage, os.getenv('DOWNLOAD_STORAGE_DIR'))


if __name__ == "__main__":
    setproctitle("ce_mounter")      # set process title

    log_config()

    # check if running as root, fail and quit if not
    if os.geteuid() != 0:           # If not root user, fail
        app_log.info("You must run this app as root, otherwise mount / umount won't work!")
        exit(1)

    # check if other instance is running, quit if it is
    if other_instance_running():
        app_log.info("Other instance is running, this instance won't run!")
        exit(1)

    # make dirs which might not exist (some might require root access)
    for one_dir in [os.getenv('MOUNT_DIR_RAW'), os.getenv('MOUNT_DIR_TRANS')]:
        os.makedirs(one_dir, exist_ok=True)

    unlink_without_fail(FILE_ROOT_DEV)          # delete this file to make get_root_fs_device() execute at least once

    settings_load()                  # load settings from disk

    sock = create_socket()              # try to create socket

    if not sock:
        app_log.error("Cannot run without socket! Terminating.")
        exit(1)

    threading.Thread(target=sock_receiver, daemon=True).start()

    app_log.info(f"MOUNT_SOCK_PATH   : {os.getenv('MOUNT_SOCK_PATH')}")
    app_log.info(f"MOUNT_DIR_RAW     : {os.getenv('MOUNT_DIR_RAW')}")
    app_log.info(f"MOUNT_DIR_TRANS   : {os.getenv('MOUNT_DIR_TRANS')}")

    app_log.info(f'On start will look for not mounted devices - they might be already connected')

    # try to mount what we can on start
    for func in [unlink_everything_translated, copy_and_symlink_config_dir,
                 mount_user_custom_folders, find_and_mount_devices, mount_shared,
                 symlink_download_storage, show_symlinked_dirs]:
        task_queue.put({'function': func, 'args': None})        # put these functions in the test queue, execute them in the worker

    threading.Thread(target=worker, daemon=True).start()        # start the task queue worker

    # watch the dev_disk_dir folder, on changes look for devices and mount them
    wm = pyinotify.WatchManager()
    loop = asyncio.get_event_loop()
    notifier = pyinotify.AsyncioNotifier(wm, loop, callback=handle_read_callback, default_proc_fun=my_process_event)

    app_log.info(f'Will now start watching folder: {DEV_DISK_DIR}')
    wm.add_watch(DEV_DISK_DIR, pyinotify.IN_CREATE | pyinotify.IN_DELETE | pyinotify.IN_UNMOUNT)

    app_log.info(f'Will now start watching folder: {os.getenv("SETTINGS_DIR")}')
    wm.add_watch(os.getenv('SETTINGS_DIR'), pyinotify.IN_CREATE | pyinotify.IN_DELETE | pyinotify.IN_UNMOUNT | pyinotify.IN_MODIFY | pyinotify.IN_CLOSE_WRITE)

    try:
        loop.run_forever()
    except KeyboardInterrupt:
        app_log.info('\nterminated by keyboard...')

    notifier.stop()
