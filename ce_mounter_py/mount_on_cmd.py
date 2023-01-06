import os
import traceback
from loguru import logger as app_log
from wrapt_timeout_decorator import timeout
from shared import umount_if_mounted, \
    unlink_without_fail, show_symlinked_dirs, MOUNT_DIR_ZIP_FILE, is_zip_mounted, \
    send_to_core


def mount_zip_file(msg):
    """ mount ZIP file if possible """
    path = msg.get('path')

    if not os.path.exists(path):  # got path, but it doesn't exist?
        app_log.warning(f'mount_zip_file: archive does not exist: {path}')
        return

    mounted = is_zip_mounted()
    app_log.info(f'mount_zip_file: is some ZIP already mounted? {mounted}')

    mount_path = MOUNT_DIR_ZIP_FILE                             # get where it should be mounted

    if mounted:
        umount_if_mounted(mount_path)       # umount dir if it's mounted

    os.makedirs(mount_path, exist_ok=True)  # create mount dir

    mount_cmd = f'fuse-zip {path} {mount_path}'  # construct mount command

    good = False
    try:  # try to mount it
        app_log.info(f'mount_zip_file: cmd: {mount_cmd}')
        os.system(mount_cmd)
        app_log.info(f'mounted {path} to {mount_path}')
        good = True
    except Exception as exc:  # on exception
        app_log.info(f'failed to mount zip with cmd: {mount_cmd} : {str(exc)}')

    # let the core know that ZIP was mounted (or failed)
    item = {'module': 'disks', 'action': 'zip_mounted', 'zip_path': path, 'mount_path': mount_path, 'success': good}
    send_to_core(item)


def unmount_folder(msg):
    """ unlink symlink, unmount device """
    unmount_path = msg.get('path')

    if not unmount_path:
        app_log.warning(f'unmount_folder: unmount_path empty? skipping')
        return

    try:
        source = None

        if os.path.exists(unmount_path):            # if the path exists
            source = os.readlink(unmount_path)      # get where the symlink is pointing
            unlink_without_fail(unmount_path)                 # remove symlink

        if source and os.path.exists(source):       # if the symlink source exists
            umount_if_mounted(source)
    except Exception as ex:
        tb = traceback.format_exc()
        app_log.warning(f'unmount_folder: failed with exception: {str(ex)}')
        app_log.warning(tb)


@timeout(10)
def mount_on_command(msg):
    """ go through list of expected commands from CE main app and execute all the found ones """

    # dictionary for commands vs their handling functions
    cmd_name_vs_handling_function = {'mount_zip': mount_zip_file, 'unmount': unmount_folder}
    cmd_name = msg.get('cmd_name')
    handler = cmd_name_vs_handling_function.get(cmd_name)

    if handler:                     # handler present?
        app_log.debug(f"mount_on_command: will call handler for cmd_name: {cmd_name}")
        handler(msg)                # call handler
        show_symlinked_dirs()       # if something was handled, show currently symlinked dirs
    else:                           # no handler?
        app_log.warning(f"mount_on_command: did not find handler for cmd_name: {cmd_name}")
