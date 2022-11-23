import os
import logging
from wrapt_timeout_decorator import timeout
from shared import print_and_log, get_symlink_path_for_letter, \
    text_from_file, FILE_MOUNT_USER, unlink_without_fail


def get_user_custom_mount_settings():
    """ read the custom user mount settings from file and return them as list """

    mnt_user = text_from_file(FILE_MOUNT_USER)              # current custom mounts

    custom_mounts = []

    if not mnt_user:                        # no custom user mounts? quit
        print_and_log(logging.INFO, f"mount_user_custom_folders: no custom user mounts from {FILE_MOUNT_USER}")
        return custom_mounts

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

        drive = drive.upper()               # drive letter to uppercase
        drive_path = (drive, custom_path)   # to tuple
        custom_mounts.append(drive_path)    # append tuple to list

    return custom_mounts


def get_user_custom_mounts_letters():
    """ get list of drive letters which custom mounts will occupy """
    custom_mounts = get_user_custom_mount_settings()        # gets list of tuples holding drive + path
    custom_letters = [mnt[0] for mnt in custom_mounts]      # get just drive letters
    return custom_letters


@timeout(10)
def mount_user_custom_folders():
    """ if the user has specified custom mounts in the FILE_MOUNT_USER (e.g. drive letter pointing to SSD drive,
        then symlink these folders as user requested """

    custom_mounts = get_user_custom_mount_settings()

    for mnt in custom_mounts:                           # go through all found custom mount settings
        drive, custom_path = mnt                        # split tuple to vars

        symlink_path = get_symlink_path_for_letter(drive)       # create path where we should symlink the user folder

        try:
            relink = False

            # if the destination exists and it's a link
            if os.path.exists(symlink_path) and os.path.islink(symlink_path):
                src_path = os.readlink(symlink_path)    # get source path

                if src_path != custom_path:             # if the link path doesn't match what should be linked here
                    relink = True
            else:           # path doesn't exist or the link is broken, try to relink
                relink = True

            if relink:                                  # if we should relink this custom mount
                unlink_without_fail(symlink_path)       # try to unlink, but expect it might fail

                os.symlink(custom_path, symlink_path)   # symlink custom path
                print_and_log(logging.DEBUG, f"mount_user_custom_folders: symlinked drive: {drive}, path: {custom_path} to {symlink_path}")
            else:                                       # should not re-link, just log message
                print_and_log(logging.DEBUG, f"mount_user_custom_folders: not re-linking {symlink_path}")
        except Exception as ex:
            print_and_log(logging.WARNING, f"mount_user_custom_folders: failed on drive: {drive}, path: {custom_path}, exception: {str(ex)}")
