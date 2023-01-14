import os
from loguru import logger as app_log
#from wrapt_timeout_decorator import timeout
from shared import get_symlink_path_for_letter, \
    text_from_file, symlink_if_needed, trigger_reload_translated


def get_user_custom_mount_settings():
    """ read the custom user mount settings from file and return them as list """

    mnt_user = text_from_file(os.getenv('FILE_MOUNT_USER'))              # current custom mounts

    custom_mounts = []

    if not mnt_user:                        # no custom user mounts? quit
        app_log.info(f"mount_user_custom_folders: no custom user mounts from {os.getenv('FILE_MOUNT_USER')}")
        return custom_mounts

    lines = mnt_user.splitlines()           # split lines in string to list of string

    for line in lines:                      # go through the individual lines
        line = line.strip()                 # remove trailing / leading spaces

        if len(line) < 3:                   # ignore lines which are too short
            continue

        if ':' not in line:
            app_log.warning(f"mount_user_custom_folders: ':' not found in line '{line}', expecting 'DRIVE:PATH'")
            continue

        drive, custom_path = line.split(':', 1)     # split line to drive and path, maximum 1 splits

        if len(drive) != 1:                 # make sure drive letter is single letter
            app_log.warning(f"mount_user_custom_folders: drive letter '{drive}' not length of 1, ignoring")
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


#@timeout(10)
def mount_user_custom_folders():
    """ if the user has specified custom mounts in the os.getenv('FILE_MOUNT_USER') (e.g. drive letter pointing to SSD drive,
        then symlink these folders as user requested """

    custom_mounts = get_user_custom_mount_settings()

    got_something = False

    for mnt in custom_mounts:                           # go through all found custom mount settings
        drive, custom_path = mnt                        # split tuple to vars

        symlink_path = get_symlink_path_for_letter(drive)       # create path where we should symlink the user folder
        good = symlink_if_needed(custom_path, symlink_path)     # create symlink, but only if needed
        got_something = got_something or good   # if at least 1 thing was symlinked, got_something will become true

    if got_something:           # if something was symlinked, the core should look for new translated drives
        trigger_reload_translated()
