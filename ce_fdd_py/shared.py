import queue
import os
from urwid_helpers import dialog
import subprocess
import codecs
import logging
from logging.handlers import RotatingFileHandler
from zipfile import ZipFile

app_log = logging.getLogger()

queue_download = queue.Queue()      # queue that holds things to download
queue_send = queue.Queue()          # queue that holds things to send to core

terminal_cols = 80  # should be 40 for ST low, 80 for ST mid
terminal_rows = 23
items_per_page = 19

main = None
main_loop = None
current_body = None
should_run = True

LOG_DIR = '/var/log/ce/'
LOG_FILE = os.path.join(LOG_DIR, 'ce_fdd_py.log')

DATA_DIR = '/var/run/ce/'
FILE_SLOTS = os.path.join(DATA_DIR, 'slots.txt')

core_sock_name = os.path.join(DATA_DIR, 'core.sock')

DOWNLOAD_STORAGE_DIR = os.path.join(DATA_DIR, 'download_storage')

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

last_status_string = ''
new_status_string = ''


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
    """ Find the storage path and return it. Use cached value if possible. """
    global last_storage_path

    last_storage_exists = last_storage_path is not None and os.path.exists(last_storage_path)  # path still valid?

    if last_storage_exists:  # while the last storage exists, keep returning it
        return last_storage_path

    storage_path = None

    # does this symlink exist?
    if os.path.exists(DOWNLOAD_STORAGE_DIR) and os.path.islink(DOWNLOAD_STORAGE_DIR):
        storage_path = os.readlink(DOWNLOAD_STORAGE_DIR)    # read the symlink

        if not os.path.exists(storage_path):                # symlink source doesn't exist? reset path to None
            storage_path = None

    if storage_path:    # if storage path was found, append subdir to it
        storage_path = os.path.join(storage_path, "fdd_imgs")
        os.makedirs(storage_path, exist_ok=True)

    # store the found storage path and return it
    last_storage_path = storage_path
    return storage_path


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


def slot_insert(slot_index, path_to_image):
    """ insert floppy image to specified slot
    :param slot_index: index of slot to eject - 0-2
    :param path_to_image: filesystem path to image file which should be inserted
    """
    global queue_send
    item = {'module': 'floppy', 'action': 'insert', 'slot': slot_index, 'image': path_to_image}
    queue_send.put(item)


def slot_eject(slot_index):
    """ eject floppy image from specified slot
    :param slot_index: index of slot to eject - 0-2
    """
    global queue_send
    item = {'module': 'floppy', 'action': 'eject', 'slot': slot_index}
    queue_send.put(item)


def file_seems_to_be_image(path_to_image, check_if_exists):
    """ check if the supplied path seems to be image or not """
    if not os.path.exists(path_to_image) or not os.path.isfile(path_to_image):
        return False, f"Error accessing {path_to_image}"

    path = path_to_image.strip()
    path = os.path.basename(path)       # get just filename
    ext = os.path.splitext(path)[1]     # get file extension

    if ext.startswith('.'):             # if extension starts with dot, remove it
        ext = ext[1:]

    ext = ext.lower()                   # to lowercase

    if ext in ['st', 'msa']:            # if extension is one of the expected values, we're good
        return True, None

    if ext != 'zip':                    # not a zip file and not any of supported extensions? fail
        return False, f"Files with '{ext}' extension not supported."

    # if we got here, it's a zip file
    try:
        with ZipFile(path_to_image, 'r') as zipObj:
            files = zipObj.namelist()       # Get list of files names in zip

            for file in files:
                if file_seems_to_be_image(file, False):
                    app_log.debug(f"the ZIP file {path_to_image} contains valid image {file}")
                    return True, None

    except Exception as ex:
        app_log.warning(f"file_seems_to_be_image failed: {str(ex)}")

    return False, "No valid image found in ZIP file"


def log_config():
    log_formatter = logging.Formatter('%(asctime)s %(levelname)s %(funcName)s(%(lineno)d) %(message)s')

    os.makedirs(LOG_DIR, exist_ok=True)
    my_handler = RotatingFileHandler(LOG_FILE, mode='a', maxBytes=1024 * 1024, backupCount=1)
    my_handler.setFormatter(log_formatter)
    my_handler.setLevel(logging.DEBUG)

    app_log = logging.getLogger()
    app_log.setLevel(logging.DEBUG)
    app_log.addHandler(my_handler)
