import os
import logging
from shared import print_and_log, get_symlink_path_for_letter, umount_if_mounted, MOUNT_COMMANDS_DIR, LETTER_ZIP, \
    text_from_file


def get_cmd_by_name(cmd_name):
    """ get command based on his name """

    path_ = os.path.join(MOUNT_COMMANDS_DIR, cmd_name)

    if not os.path.exists(path_):           # path doesn't exist? just quit
        return None

    path_in_cmd = text_from_file(path_)     # read content of file

    try:
        os.unlink(path_)                    # delete command file after reading
    except Exception as ex:
        print_and_log(logging.WARNING, f'get_cmd_by_name: failed to unlink {path_}: {str(ex)}')

    return path_in_cmd                      # return file path


def mount_zip_file(zip_file_path):
    """ mount ZIP file if possible """
    if not os.path.exists(zip_file_path):  # got path, but it doesn't exist?
        return

    mount_path = get_symlink_path_for_letter(LETTER_ZIP)  # get where it should be mounted
    umount_if_mounted(mount_path)           # umount dir if it's mounted
    os.makedirs(mount_path, exist_ok=True)  # create mount dir

    mount_cmd = f'fuse-zip {zip_file_path} {mount_path}'  # construct mount command

    try:  # try to mount it
        print_and_log(logging.INFO, f'mount_zip_file: cmd: {mount_cmd}')
        os.system(mount_cmd)
        print_and_log(logging.INFO, f'mounted {zip_file_path} to {mount_path}')
    except Exception as exc:  # on exception
        print_and_log(logging.INFO, f'failed to mount zip with cmd: {mount_cmd} : {str(exc)}')


def unmount_folder(unmount_path):
    """ unlink symlink, unmount device """
    try:
        source = None

        if os.path.exists(unmount_path):            # if the path exists
            source = os.readlink(unmount_path)      # get where the symlink is pointing
            os.unlink(unmount_path)                 # remove symlink

        if source and os.path.exists(source):       # if the symlink source exists
            umount_if_mounted(unmount_path)
    except Exception as ex:
        print_and_log(logging.WARNING, f'unmount_folder: failed with exception: {str(ex)}')


def mount_on_command():
    """ endless loop to mount things from CE main app on request """
    if not os.path.exists(MOUNT_COMMANDS_DIR):              # if dir for commands doesn't exist, create it
        os.makedirs(MOUNT_COMMANDS_DIR, exist_ok=True)

    # command to mount ZIP?
    zip_file_path = get_cmd_by_name('mount_zip')

    if zip_file_path:       # should mount ZIP?
        mount_zip_file(zip_file_path)

    # command to unmount folder / drive?
    unmount_cmd = get_cmd_by_name('unmount')

    if unmount_cmd:
        unmount_folder(unmount_cmd)
