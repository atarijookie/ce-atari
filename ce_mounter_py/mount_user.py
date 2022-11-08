import os
import logging
from wrapt_timeout_decorator import timeout
from shared import print_and_log, get_symlink_path_for_letter, \
    text_to_file, text_from_file, FILE_MOUNT_USER, FILE_MOUNT_USER_LAST


@timeout(10)
def mount_user_custom_folders():
    """ if the user has specified custom mounts in the FILE_MOUNT_USER (e.g. drive letter pointing to SSD drive,
        then symlink these folders as user requested """

    mnt_user = text_from_file(FILE_MOUNT_USER)              # current custom mounts
    mnt_user_last = text_from_file(FILE_MOUNT_USER_LAST)    # last user mounts

    if not mnt_user:                        # no custom user mounts? quit
        print_and_log(logging.INFO, f"mount_user_custom_folders: no custom user mounts from {FILE_MOUNT_USER}")
        return

    if mnt_user == mnt_user_last:           # check if custom mounts changed, quit if not
        print_and_log(logging.INFO, f"mount_user_custom_folders: custom mounts not changed, not mounting")
        return

    text_to_file(mnt_user, FILE_MOUNT_USER_LAST)            # store current custom mounts to last ones

    lines = mnt_user.splitlines()           # split lines in string to list of string

    for line in lines:                      # go through the individual lines
        line = line.strip()                 # remove trailing / leading spaces

        if len(line) < 3:                   # ignore lines which are too short
            continue

        if ':' not in line:
            print_and_log(logging.WARNING, f"mount_user_custom_folders: ':' not found in line '{line}', expecting 'DRIVE:PATH'")
            continue

        drive, custom_path = line.split(':', 1)     # split line to drive and path, maximum 1 splits

        if len(drive) != 1:                 # make sure drive letter is single letter
            print_and_log(logging.WARNING, f"mount_user_custom_folders: drive letter '{drive}' not length of 1, ignoring")
            continue

        drive = drive.upper()                       # drive letter to uppercase

        symlink_path = get_symlink_path_for_letter(drive)       # create path where we should symlink the user folder

        try:
            if os.path.exists(symlink_path):            # delete the destrination if it already exists
                os.unlink(symlink_path)

            os.symlink(custom_path, symlink_path)       # symlink custom path
            print_and_log(logging.DEBUG, f"mount_user_custom_folders: symlinked drive: {drive}, path: {custom_path} to {symlink_path}")
        except Exception as ex:
            print_and_log(logging.WARNING, f"mount_user_custom_folders: failed on drive: {drive}, path: {custom_path}, exception: {str(ex)}")
