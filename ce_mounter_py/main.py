import os
import pyinotify
import asyncio
import logging
import threading, queue
import traceback
from functools import partial
from setproctitle import setproctitle
from wrapt_timeout_decorator import timeout
from shared import print_and_log, log_config, DEV_DISK_DIR, MOUNT_DIR_RAW, MOUNT_COMMANDS_DIR, \
    SETTINGS_PATH, MOUNT_DIR_TRANS, CONFIG_PATH_SOURCE, CONFIG_PATH_COPY, \
    get_symlink_path_for_letter, setting_get_bool, unlink_everything_translated, letter_confdrive, letter_shared, \
    letter_zip, unlink_without_fail, unlink_everything_raw, settings_load
from mount_usb_trans import get_usb_devices, find_and_mount_translated
from mount_usb_raw import find_and_mount_raw
from mount_hdd_image import mount_hdd_image
from mount_on_cmd import mount_on_command
from mount_shared import mount_shared
from mount_user import mount_user_custom_folders

task_queue = queue.Queue()

# TODO: test / fix together with ce_conf_py - investigate lagging


def worker():
    lock = threading.Lock()

    while True:
        funct = task_queue.get()
        lock.acquire()
        print_and_log(logging.INFO, f'')
        print_and_log(logging.INFO, f'EXECUTING: {funct.__name__}()')

        try:
            funct()
        except TimeoutError:
            print_and_log(logging.WARNING, f'The function {funct.__name__} was terminated with TimeoutError')
        except Exception as ex:
            print_and_log(logging.WARNING, f'The function {funct.__name__} has crashed: {type(ex).__name__} - {str(ex)}')
            tb = traceback.format_exc()
            print_and_log(logging.WARNING, tb)

        lock.release()
        task_queue.task_done()


def handle_read_callback(ntfr):
    print_and_log(logging.INFO, f'monitored folder changed...')

    for path_, handler in watched_paths.items():    # go through the watched paths
        if event_source.get(path_):                 # if this one did change
            event_source[path_] = False             # clear this flag
            print_and_log(logging.INFO, f'adding handler to task queue: {handler.__name__}()')
            task_queue.put(handler)                 # call the handler


def reload_settings_mount_everything():
    """ this handler gets called when settings change, reload settings and try mounting
    if settings for mounts changed """

    changed_letters, changed_ids = None, None

    try:
        changed_letters, changed_ids = settings_load()
    except Exception as ex:
        print_and_log(logging.ERROR, f'settings_load excepsion: {str(ex)}')

    if changed_letters:             # if drive letters changed, unlink everything translated
        unlink_everything_translated()

    if changed_ids:                 # if ACSI ID types changed, unlink linked raw devices
        unlink_everything_raw()

    mount_user_custom_folders()     # mount user custom folders if they changed
    copy_and_symlink_config_dir()   # possibly symlink config drive
    mount_shared()                  # either remount shared drive or just symlink it
    find_and_mount_devices()        # mount and/or symlink USB devices
    show_symlinked_dirs()           # show the mounts after possible remounts


@timeout(10)
def find_and_mount_devices():
    """ look for USB devices, find those which are not mounted yet, find a mount point for them, mount them """
    mount_raw_not_trans = setting_get_bool('MOUNT_RAW_NOT_TRANS')
    print_and_log(logging.INFO, f"MOUNT mode: {'RAW' if mount_raw_not_trans else 'TRANS'}")

    root_devs, part_devs = get_usb_devices()            # get attached USB devices
    print_and_log(logging.INFO, f'devices: {root_devs}')

    if mount_raw_not_trans:         # for raw mount, check if symlinked
        find_and_mount_raw(root_devs)
    else:                           # for translated mounts
        find_and_mount_translated(root_devs, part_devs)

    mount_hdd_image()               # also try to mount HDD image


# following two will help us to determine who caused the event and what function should handle it
event_source = {}
watched_paths = {SETTINGS_PATH: reload_settings_mount_everything,
                 MOUNT_COMMANDS_DIR: mount_on_command,
                 DEV_DISK_DIR: find_and_mount_devices}


def my_process_event(event):
    """ this event processor is called by pynotify when it processes an event, and we look here into the
    path which caused this event and then mark that handler for this path should be executed
    """

    # print_and_log(logging.INFO, f"my_process: {event.__dict__}")

    for path_ in watched_paths.keys():      # go through the watched paths
        if event.path == path_:             # if this watched patch caused this event
            event_source[path_] = True      # mark this event source


def copy_and_symlink_config_dir():
    """ create a copy of configdir, then symlink it to right place """
    if not os.path.exists(CONFIG_PATH_SOURCE):
        print_and_log(logging.WARNING, f"Config drive origin folder doesn't exist! ( {CONFIG_PATH_SOURCE} )")
        return

    try:
        os.makedirs(CONFIG_PATH_COPY, exist_ok=True)                    # create dir for copy
        os.system(f"cp -r {CONFIG_PATH_SOURCE}/* {CONFIG_PATH_COPY}")   # copy original config dir to copy dir

        symlink_path = get_symlink_path_for_letter(letter_confdrive())

        if os.path.exists(symlink_path):
            unlink_without_fail(symlink_path)

        os.symlink(CONFIG_PATH_COPY, symlink_path)                      # symlink copy to correct mount dir
        print_and_log(logging.INFO, f"Config drive was symlinked to: {symlink_path}")
    except Exception as ex:
        print_and_log(logging.WARNING, f'copy_and_symlink_config_dir: failed with: {type(ex).__name__} - {str(ex)}')


def get_dir_usage(custom_letters, search_dir, name):
    """ turn folder name (drive letter) into what is this folder used for - config / shared / usb / zip drive """
    name_to_usage = {letter_shared(): "shared drive",
                     letter_confdrive(): "config drive",
                     letter_zip(): "ZIP file drive"}

    for c_letter in custom_letters:                             # insert custom letters into name_to_usage
        name_to_usage[c_letter] = "custom drive"

    usage = name_to_usage.get(name, "USB drive")

    fullpath = os.path.join(search_dir, name)       # create full path to this dir

    if os.path.islink(fullpath):                    # if this is a link, read source of the link
        fullpath = os.readlink(fullpath)

    res = f"({usage})".ljust(20) + fullpath
    return res


def get_symlink_source(search_dir, name):
    fullpath = os.path.join(search_dir, name)       # create full path to this dir

    if os.path.islink(fullpath):                    # if this is a link, read source of the link
        fullpath = os.readlink(fullpath)

    res = f" ".ljust(20) + fullpath
    return res


def get_and_show_symlinks(search_dir, fun_on_each_found):
    dirs = []

    for name in os.listdir(search_dir):         # go through the dir
        dirs.append(name)                       # append to list of found

    if dirs:                                    # something was found?
        dirs = sorted(dirs)                     # sort results

        for one_dir in dirs:
            desc = '' if not fun_on_each_found else fun_on_each_found(search_dir, one_dir)  # get description if got function
            print_and_log(logging.INFO, f" * {one_dir} {desc}")
    else:                                       # nothing was found
        print_and_log(logging.INFO, f" (none)")


def show_symlinked_dirs():
    """ prints currently mounted / symlinked dirs """

    from mount_user import get_user_custom_mounts_letters
    custom_letters = get_user_custom_mounts_letters()           # fetch all the user custom letters

    # first show translated drives
    print_and_log(logging.INFO, "\nlist of current translated drives:")
    get_and_show_symlinks(MOUNT_DIR_TRANS, partial(get_dir_usage, custom_letters))

    # then show RAW drives
    print_and_log(logging.INFO, "\nlist of current RAW drives:")
    get_and_show_symlinks(MOUNT_DIR_RAW, get_symlink_source)

    print_and_log(logging.INFO, " ")


if __name__ == "__main__":
    setproctitle("ce_mounter")      # set process title

    log_config()

    # check if running as root, fail and quit if not
    if os.geteuid() != 0:           # If not root user, fail
        print_and_log(logging.INFO, "\nYou must run this app as root, otherwise mount / umount won't work!")
        exit(1)

    # make dirs which might not exist (some might require root access)
    os.makedirs(MOUNT_DIR_RAW, exist_ok=True)
    os.makedirs(MOUNT_DIR_TRANS, exist_ok=True)
    os.makedirs(MOUNT_COMMANDS_DIR, exist_ok=True)

    settings_load()                  # load settings from disk

    print_and_log(logging.INFO, f"MOUNT_COMMANDS_DIR: {MOUNT_COMMANDS_DIR}")
    print_and_log(logging.INFO, f"MOUNT_DIR_RAW     : {MOUNT_DIR_RAW}")
    print_and_log(logging.INFO, f"MOUNT_DIR_TRANS   : {MOUNT_DIR_TRANS}")

    print_and_log(logging.INFO, f'On start will look for not mounted devices - they might be already connected')

    # try to mount what we can on start
    for func in [unlink_everything_translated, copy_and_symlink_config_dir, mount_user_custom_folders, find_and_mount_devices,
                 mount_shared, mount_on_command, show_symlinked_dirs]:
        task_queue.put(func)        # put these functions in the test queue, execute them in the worker

    threading.Thread(target=worker, daemon=True).start()        # start the task queue worker

    # watch the dev_disk_dir folder, on changes look for devices and mount them
    wm = pyinotify.WatchManager()
    loop = asyncio.get_event_loop()
    notifier = pyinotify.AsyncioNotifier(wm, loop, callback=handle_read_callback, default_proc_fun=my_process_event)

    print_and_log(logging.INFO, f'Will now start watching folder: {DEV_DISK_DIR}')
    wm.add_watch(DEV_DISK_DIR, pyinotify.IN_CREATE | pyinotify.IN_DELETE | pyinotify.IN_UNMOUNT)

    print_and_log(logging.INFO, f'Will now start watching folder: {SETTINGS_PATH}')
    wm.add_watch(SETTINGS_PATH, pyinotify.IN_CREATE | pyinotify.IN_DELETE | pyinotify.IN_UNMOUNT | pyinotify.IN_MODIFY | pyinotify.IN_CLOSE_WRITE)

    print_and_log(logging.INFO, f'Will now start watching folder: {MOUNT_COMMANDS_DIR}')
    wm.add_watch(MOUNT_COMMANDS_DIR, pyinotify.IN_CREATE | pyinotify.IN_DELETE | pyinotify.IN_UNMOUNT | pyinotify.IN_MODIFY | pyinotify.IN_CLOSE_WRITE)

    try:
        loop.run_forever()
    except KeyboardInterrupt:
        print_and_log(logging.INFO, '\nterminated by keyboard...')

    notifier.stop()
