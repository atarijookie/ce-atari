import os
import pyinotify
import asyncio
from setproctitle import setproctitle
import logging
from shared import print_and_log, log_config, settings_load, DEV_DISK_DIR, MOUNT_DIR_RAW, MOUNT_COMMANDS_DIR, \
    SETTINGS_PATH, MOUNT_DIR_TRANS
from mount_usb import find_and_mount_devices
from mount_on_cmd import mount_on_command
from mount_shared import mount_shared


def handle_read_callback(ntfr):
    print_and_log(logging.INFO, f'monitored folder changed...')

    for path_, handler in watched_paths.items():    # go through the watched paths
        if event_source.get(path_):                 # if this one did change
            event_source[path_] = False             # clear this flag
            print_and_log(logging.INFO, f'will now call: {handler}')
            handler()                               # call the handler


def reload_settings_mount_shared():
    """ this handler gets called when settings change, reload settings and try mounting
    if settings for mounts changed """

    changed_usb, changed_shared = settings_load()

    if changed_shared:      # if settings for shared drive changed, try u/mount shared drive
        print_and_log(logging.INFO, 'shared drive settings changed, will call mount_shared()')
        mount_shared()
    else:
        print_and_log(logging.INFO, 'shared drive settings NOT changed')

    if changed_usb:         # if settings for usb drive changed, try u/mount sub drive
        print_and_log(logging.INFO, 'USB drive related settings changed, will call find_and_mount_devices()')
        # TODO: possibly umount all
        find_and_mount_devices()
    else:
        print_and_log(logging.INFO, 'USB drive related settings NOT changed')


# following two will help us to determine who caused the event
event_source = {}
watched_paths = {SETTINGS_PATH: reload_settings_mount_shared,
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


if __name__ == "__main__":
    setproctitle("ce_mounter")      # set process title

    log_config()

    os.makedirs(MOUNT_DIR_RAW, exist_ok=True)
    os.makedirs(MOUNT_DIR_TRANS, exist_ok=True)

    # check if running as root, fail and quit if not
    if os.geteuid() != 0:           # If not root user, fail
        print_and_log(logging.INFO, "\nYou must run this app as root, otherwise mount / umount won't work!")
        exit(1)

    settings_load()                 # load settings from disk

    print_and_log(logging.INFO, f'On start will look for not mounted devices - they might be already connected')

    for handler in watched_paths.values():      # call all handlers before doing them only on event
        handler()

    # TODO: remove broken linked devices / mounts on removal

    # TODO: if drive letters / raw IDs change, we should umount + unlink all, then re-mount + re-link

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
