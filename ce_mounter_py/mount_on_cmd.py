import os
import logging
from shared import print_and_log, get_symlink_path_for_letter, umount_if_mounted, MOUNT_COMMANDS_DIR, LETTER_ZIP


def get_mount_zip_cmd():
    """ get which ZIP file should be mounted """

    path_ = os.path.join(MOUNT_COMMANDS_DIR, "mount_zip")

    if not os.path.exists(path_):       # no file like this exists? quit
        return None

    try:
        with open(path_) as f:          # open and read file
            line = f.read()

        os.unlink(path_)                # delete the file

        line = line.strip()             # remove whitespaces
        return line
    except Exception as exc:            # on exception
        print_and_log(logging.INFO, f'failed to read {path_} : {str(exc)}')

    return None                         # if got here, it failed


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


def mount_on_command():
    """ endless loop to mount things from CE main app on request """
    if not os.path.exists(MOUNT_COMMANDS_DIR):              # if dir for commands doesn't exist, create it
        os.makedirs(MOUNT_COMMANDS_DIR, exist_ok=True)

    #  command to mount ZIP?
    zip_file_path = get_mount_zip_cmd()

    if zip_file_path:       # should mount ZIP?
        mount_zip_file(zip_file_path)
