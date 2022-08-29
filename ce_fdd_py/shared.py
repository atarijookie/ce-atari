import queue
import os
from urwid_helpers import dialog
import subprocess
import codecs
import logging

app_log = logging.getLogger()

queue_download = queue.Queue()      # queue that holds things to download

terminal_cols = 80  # should be 40 for ST low, 80 for ST mid
terminal_rows = 23
items_per_page = 19

main = None
main_loop = None
current_body = None
should_run = True

PATH_VAR = '/var/run/ce'
FILE_SLOTS = PATH_VAR + '/slots.txt'

PATH_TO_LISTS = "/ce/lists/"                                    # where the lists are stored locally
BASE_URL = "http://joo.kie.sk/cosmosex/update/"                 # base url where the lists will be stored online
LIST_OF_LISTS_FILE = "list_of_lists.csv"
LIST_OF_LISTS_URL = BASE_URL + LIST_OF_LISTS_FILE               # where the list of lists is on web
LIST_OF_LISTS_LOCAL = PATH_TO_LISTS + LIST_OF_LISTS_FILE        # where the list of lists is locally

list_of_lists = []      # list of dictionaries: {name, url, filename}
list_index = 0          # index of list in list_of_lists which will be worked on
list_of_items = []      # list containing all the items from file (unfiltered)
list_of_items_filtered = []     # filtered list of items (based on search string)
pile_current_page = None        # urwid pile containing buttons for current page
search_phrase = ""

main_loop = None        # main loop of the urwid library
text_pages = None       # widget holding the text showing current and total pages
page_current = 1        # currently shown page
text_status = None      # widget holding status text
last_focus_path = None  # holds last focus path to widget which had focus before going to widget subpage
terminal_cols = 80      # should be 40 for ST low, 80 for ST mid
terminal_rows = 23
items_per_page = terminal_rows - 4
last_storage_path = None

on_unhandled_keys_handler = None
view_object = None


def on_unhandled_keys_generic(key):
    # generic handler for unhandled keys, will be assigned on start...
    # later on, when you want to handle unhandled keys on specific screen, set the on_unhandled_keys_handler
    # to handling function

    if on_unhandled_keys_handler:
        on_unhandled_keys_handler(key)


def show_storage_read_only():
    """ show warning message that cannot write storage """
    global main_loop, current_body
    dialog(main_loop, current_body,
           "Cannot write to storage path.\nYou won't be able to download images, "
           "and even might have issues loading your images.")


def show_no_storage():
    """ show warning message that no storage is found """
    global main_loop, current_body
    dialog(main_loop, current_body,
           f"Failed to get storage path. Attach USB drive or shared network drive and try again.")


def can_write_to_storage(path):
    """ check if can write to storage path """
    test_file = os.path.join(path, "test_file.txt")

    try:        # first remove the test file if it exists
        os.remove(test_file)
    except:     # don't fail if failed to remove file - might not exist at this time
        pass

    good = False

    try:        # create file and write to file
        with open(test_file, 'wt') as out:
            out.write("whatever")

        good = True
    except:     # this error is expected
        pass

    try:        # remove the test file if it exists
        os.remove(test_file)
    except:     # don't fail if failed to remove file - might not exist at this time
        pass

    return good


def get_storage_path():
    global last_storage_path

    last_storage_exists = last_storage_path is not None and os.path.exists(last_storage_path)  # path still valid?

    if last_storage_exists:  # while the last storage exists, keep returning it
        return last_storage_path

    # last returned path doesn't exist, check current mounts
    result = subprocess.run(['mount'], stdout=subprocess.PIPE)
    result = codecs.decode(result.stdout)
    mounts = result.split("\n")

    storages = []
    storage_shared = None  # holds path to shared drive
    storage_drive = None  # holds path to first USB storage drive

    for mount in mounts:  # go through the mount lines
        if '/mnt/' not in mount:  # if this whole line does not contain /mnt/ , skip it
            continue

        mount_parts = mount.split(' ')  # split line into parts

        for part in mount_parts:  # find the part that contains the '/mnt/' part
            if '/mnt/' not in part:  # if this is NOT the /mnt/ part, skip it
                continue

            part = os.path.join(part, "floppy_images")  # add floppy images dir to path part (might not exist yet)
            storages.append(part)

            if '/mnt/shared' in part:  # is this a shared mount? remember this path
                storage_shared = part
            else:  # this is a USB drive mount
                if not storage_drive:  # don't have first drive yet? remember this path
                    storage_drive = part

    if not storages:  # no storage found? fail here
        last_storage_path = None
        return None

    # if we got here, we definitelly have some storages, but we might not have the required subdir
    # so first check if any of the found storages has the subdir already, and if it does, then use it
    for storage in storages:  # go through the storages and check if the floppy_images subdir exists
        if os.path.exists(storage):  # found storage with existing subdir, use it
            last_storage_path = storage
            return storage

    # if we got here, we got some storages, but none of them has floppy_images subdir, so create one and return path
    storage_use = storage_drive if storage_drive else storage_shared  # use USB drive first if available, otherwise use shared drive

    subprocess.run(['mkdir', '-p', storage_use])  # create the subdir
    last_storage_path = storage_use  # remember what we've returned
    return storage_use  # return it


def load_list_from_csv(csv_filename):
    list_of_items = []

    app_log.debug(f'will load .csv: {csv_filename}')

    # read whole file into memory, split to lines
    file = open(csv_filename, "r")
    data = file.read()
    file.close()

    data = data.replace("<br>", "\n")
    lines = data.split("\n")

    # go through the lines, extract individual items
    for line in lines:
        cols = line.split(",", 2)                       # split to 3 items - url, crc, content (which is also coma-separated, but we want it as 1 piece here)

        if len(cols) < 3:                               # not enough cols in this row? skip it
            continue

        item = {'url': cols[0], 'crc': cols[1], 'content': cols[2]}     # add {name, url} to item

        url_filename = os.path.basename(item['url'])    # get filename from url
        item['filename'] = url_filename

        list_of_items.append(item)

    return list_of_items
