import os
import traceback
from loguru import logger as app_log
from wrapt_timeout_decorator import timeout
from shared import umount_if_mounted, \
    unlink_without_fail, show_symlinked_dirs, MOUNT_DIR_ZIP_FILE, is_zip_mounted, \
    send_to_core, readlink_without_fail, MOUNTED_ZIP_FILE_LINK, symlink_if_needed


def unmount_zip_file_if_source_not_exists():
    """ This function should be called when some USB / network drive is removed from the system
        and it will check if the currently mounted ZIP file still exists (and thus can be accessed)
        or it disappeared with the drive and thus the ZIP file needs to be unmounted, too.
    """

    zip_path_old = readlink_without_fail(MOUNTED_ZIP_FILE_LINK)     # get link source or None

    if not zip_path_old:                # link couldn't be read? we can skip the rest
        unlink_without_fail(MOUNTED_ZIP_FILE_LINK)
        return

    if os.path.exists(zip_path_old):    # path exists? ignore the rest, everything OK
        return

    # at this point we got the link source, but it doesn't exist

    mount_path = MOUNT_DIR_ZIP_FILE             # get where it should be mounted
    umount_if_mounted(mount_path)               # try to umount it if possible
    unlink_without_fail(MOUNTED_ZIP_FILE_LINK)  # we can now remove the link to ZIP file

    # let the core know that ZIP was unmounted
    item = {'module': 'disks', 'action': 'zip_unmounted', 'zip_path': zip_path_old, 'mount_path': mount_path}
    send_to_core(item)


def mount_zip_file(msg):
    """ mount ZIP file if possible """
    zip_path_new = msg.get('path')

    if not os.path.exists(zip_path_new):  # got path, but it doesn't exist?
        app_log.warning(f'mount_zip_file: archive does not exist: {zip_path_new}')
        return

    mounted = is_zip_mounted()              # check if some ZIP is still mounted
    zip_path_old = readlink_without_fail(MOUNTED_ZIP_FILE_LINK)     # get link source or None

    app_log.info(f'mount_zip_file: is some ZIP already mounted? {mounted}, source: {zip_path_old}')

    if zip_path_new == zip_path_old:        # zip file not changed? ignore rest
        app_log.info(f'mount_zip_file: this ZIP file: {zip_path_old} is already mounted, not remounting')
        return

    mount_path = MOUNT_DIR_ZIP_FILE         # get where it should be mounted

    if mounted:
        umount_if_mounted(mount_path)       # umount dir if it's mounted

        # let the core know that ZIP was unmounted
        item = {'module': 'disks', 'action': 'zip_unmounted', 'zip_path': zip_path_old, 'mount_path': mount_path}
        send_to_core(item)

    os.makedirs(mount_path, exist_ok=True)  # create mount dir

    mount_cmd = f'fuse-zip {zip_path_new} {mount_path}'  # construct mount command

    good = False
    try:  # try to mount it
        app_log.info(f'mount_zip_file: cmd: {mount_cmd}')
        os.system(mount_cmd)
        app_log.info(f'mounted {zip_path_new} to {mount_path}')
        symlink_if_needed(zip_path_new, MOUNTED_ZIP_FILE_LINK)      # store link to currently mounted ZIP file
        good = True
    except Exception as exc:  # on exception
        app_log.info(f'failed to mount zip with cmd: {mount_cmd} : {str(exc)}')

    # let the core know that ZIP was mounted (or failed)
    item = {'module': 'disks', 'action': 'zip_mounted', 'zip_path': zip_path_new, 'mount_path': mount_path, 'success': good}
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
