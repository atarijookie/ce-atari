import os
import logging
from glob import glob
from shared import print_and_log, letter_shared, get_usb_drive_letters, get_symlink_path_for_letter, \
    text_to_file, load_one_setting


def resolve_hddimage_path(path_image: str):
    """ Function will try to resolve the image path from supported patterns to a file which can be symlinked.

    Supported patterns are in the path_image are:
        - just absolute path - if you specify absolute image path anywhere on the disk and it's reachable, will use that
        - 'usb/' prefix      - search only on USB drives
        - 'shared/' prefix   - search only on shared (network) drive
        - no prefix          - will search all the attached drives
        - '*' + '?' chars    - they will be expanded to try to match existing files
    """

    # path exists and it's a file? use it as-is
    if os.path.exists(path_image) and os.path.isfile(path_image):
        print_and_log(logging.INFO, f"resolve_hddimage_path: HDD image path {path_image} exists, using as is")
        return path_image

    # the image path doesn't look like absolute path (or at least it doesn't point to existing file), so let's
    # try to use it as relative path

    if path_image.startswith("shared"):                         # should be looking for image only on shared drive?
        drive_letters = [letter_shared()]                       # drive letter just for shared drive
        print_and_log(logging.INFO, f"resolve_hddimage_path: will look for {path_image} on shared drive: {letter_shared()}")
        path_image = path_image[len("shared/"):]                # get part of path after 'shared/' prefix
    elif path_image.startswith("usb"):                          # should be looking for image only on usb drive?
        drive_letters = get_usb_drive_letters(True)             # get all the possible USB drive letters
        print_and_log(logging.INFO, f"resolve_hddimage_path: will look for {path_image} on USB drives: {drive_letters}")
        path_image = path_image[len("usb/"):]                   # get part of path after 'shared/' prefix
    else:                                                       # will be looking for image on any drive
        drive_letters = [chr(65 + i) for i in range(2, 16)]     # generate drive letters from C to P
        print_and_log(logging.INFO, f"resolve_hddimage_path: will look for {path_image} on ALL drives")

    # go through all the drive letters we should try to use
    for letter in drive_letters:
        path_drive = get_symlink_path_for_letter(letter)    # get path for this drive letter

        if not os.path.exists(path_drive):                  # skip this drive if it doesn't exist
            continue

        full_path = os.path.join(path_drive, path_image)    # join drive path with relative image path

        paths = glob(full_path)     # do the unix style pattern expansion, returns list of matching files

        for path in paths:          # go through all the matching patterns
            if os.path.exists(path) and os.path.isfile(path):   # path exists and it's a file? good, return it
                print_and_log(logging.INFO, f"resolve_hddimage_path: resolved {path_image} to {path}")
                return path

    # if came here, nothing found, return None
    print_and_log(logging.INFO, f"resolve_hddimage_path: couldn't resolve {path_image} any valid path")
    return None


def get_hddimage_path():
    """ get hdd image path from settings, check if the file exists, return it if it exists """
    hdd_image = load_one_setting('HDDIMAGE')        # get value from settings
    print_and_log(logging.DEBUG, f"resolve_hddimage_path: HDDIMAGE {hdd_image}")

    if not hdd_image:                       # no hdd image in settings? just quit
        print_and_log(logging.DEBUG, f"resolve_hddimage_path: empty, not returning")
        return None

    # do path resolution here
    hdd_image = resolve_hddimage_path(hdd_image)

    if not hdd_image or not os.path.exists(hdd_image):  # if te file doesn't exist, return None
        print_and_log(logging.DEBUG, f"resolve_hddimage_path: file {hdd_image} doesn't exist")
        text_to_file("FAIL", os.getenv('FILE_HDDIMAGE_RESOLVED'))        # store nothing to resolved hdd image file
        return None

    print_and_log(logging.DEBUG, f"resolve_hddimage_path: file {hdd_image} exist, will use it")
    text_to_file(hdd_image, os.getenv('FILE_HDDIMAGE_RESOLVED'))     # store hdd_image to resolved hdd image file
    return hdd_image                                    # return path to file


def mount_hdd_image():
    """ symlink HDD image to configured ACSI slot """

    hdd_image_path = get_hddimage_path()    # get path to hdd image

    if not hdd_image_path:                  # don't have path? just quit
        print_and_log(logging.INFO, f"mount_hdd_image: no valid image path, not symlinking")
        return

    print_and_log(logging.INFO, f"mount_hdd_image: will symlink {hdd_image_path} if needed now")

    from mount_usb_raw import find_and_mount_raw
    find_and_mount_raw([hdd_image_path])    # send the hdd image to be symlinked
