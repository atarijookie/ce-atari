import os
import logging
import traceback
from wrapt_timeout_decorator import timeout
from shared import print_and_log, get_symlink_path_for_letter, umount_if_mounted, \
    text_from_file, letter_zip, unlink_without_fail, show_symlinked_dirs, MOUNT_DIR_ZIP_FILE, is_zip_mounted, \
    symlink_if_needed


def get_cmd_by_name(cmd_name):
    """ get command based on his name """

    path_ = os.path.join(os.getenv('MOUNT_COMMANDS_DIR'), cmd_name)

    if not os.path.exists(path_):           # path doesn't exist? just quit
        return None

    path_in_cmd = text_from_file(path_)     # read content of file
    unlink_without_fail(path_)              # delete command file after reading
    return path_in_cmd                      # return file path


def mount_zip_file(zip_file_path):
    """ mount ZIP file if possible """
    if not os.path.exists(zip_file_path):  # got path, but it doesn't exist?
        return

    mounted = is_zip_mounted()
    print_and_log(logging.INFO, f'mount_zip_file: is some ZIP already mounted? {mounted}')

    mount_path = MOUNT_DIR_ZIP_FILE                             # get where it should be mounted
    symlink_path = get_symlink_path_for_letter(letter_zip())    # get where it should be symlinked

    unlink_without_fail(symlink_path)       # unlink symlink if it exists

    if mounted:
        umount_if_mounted(mount_path)       # umount dir if it's mounted

    os.makedirs(mount_path, exist_ok=True)  # create mount dir

    mount_cmd = f'fuse-zip {zip_file_path} {mount_path}'  # construct mount command

    try:  # try to mount it
        print_and_log(logging.INFO, f'mount_zip_file: cmd: {mount_cmd}')
        os.system(mount_cmd)
        print_and_log(logging.INFO, f'mounted {zip_file_path} to {mount_path}')

        symlink_if_needed(mount_path, symlink_path)     # create symlink, but only if needed
    except Exception as exc:  # on exception
        print_and_log(logging.INFO, f'failed to mount zip with cmd: {mount_cmd} : {str(exc)}')


def unmount_folder(unmount_path):
    """ unlink symlink, unmount device """
    try:
        source = None

        if os.path.exists(unmount_path):            # if the path exists
            source = os.readlink(unmount_path)      # get where the symlink is pointing
            unlink_without_fail(unmount_path)                 # remove symlink

        if source and os.path.exists(source):       # if the symlink source exists
            umount_if_mounted(source)
    except Exception as ex:
        tb = traceback.format_exc()
        print_and_log(logging.WARNING, f'unmount_folder: failed with exception: {str(ex)}')
        print_and_log(logging.WARNING, tb)


@timeout(10)
def mount_on_command():
    """ go through list of expected commands from CE main app and execute all the found ones """
    if not os.path.exists(os.getenv('MOUNT_COMMANDS_DIR')):              # if dir for commands doesn't exist, create it
        os.makedirs(os.getenv('MOUNT_COMMANDS_DIR'), exist_ok=True)

    # dictionary for commands vs their handling functions
    cmd_name_vs_handling_function = {'mount_zip': mount_zip_file, 'unmount': unmount_folder}

    handled = False         # something was handled?

    # go through the supported commands and handle them if needed
    for cmd_name, handling_fun in cmd_name_vs_handling_function.items():
        cmd_args = get_cmd_by_name(cmd_name)        # try to fetch cmd args for this command

        if cmd_args:                    # got cmd args? then we should execute handling function
            print_and_log(logging.INFO, f"mount_on_command: for command {cmd_name} found args: {cmd_args}, will handle")
            handling_fun(cmd_args)
            handled = True
        else:
            print_and_log(logging.INFO, f"mount_on_command: skipping command {cmd_name}")

    # if something was handled, show currently symlinked dirs
    if handled:
        show_symlinked_dirs()
