import queue
import os
import json
from urllib.request import urlopen
from urllib.parse import urlencode
from urwid_helpers import dialog
import subprocess
import codecs
import socket
import logging
from logging.handlers import RotatingFileHandler
from zipfile import ZipFile
from dotenv import load_dotenv

app_log = logging.getLogger()

main = None
current_body = None
should_run = True

list_index = 0          # index of list in list_of_lists which will be worked on
pile_current_page = None        # urwid pile containing buttons for current page
search_phrase = ""

main_loop = None        # main loop of the urwid library
text_pages = None       # widget holding the text showing current and total pages
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


def load_dotenv_config():
    """ Try to load the dotenv configuration file.
    First try to see if there's an override path for this config file specified in the env variables.
    Then try the normal installation path for dotenv on ce: /ce/services/.env
    If that fails, try to find and use local dotenv file used during development - .env in your local dir
    """

    # First try to see if there's an override path for this config file specified in the env variables.
    path = os.environ.get('CE_DOTENV_PATH')

    if path and os.path.exists(path):       # path in env found and it really exists, use it
        load_dotenv(dotenv_path=path)
        return

    # Then try the normal installation path for dotenv on ce: /ce/services/.env
    ce_dot_env_file = '/ce/services/.env'
    if os.path.exists(ce_dot_env_file):
        load_dotenv(dotenv_path=ce_dot_env_file)
        return

    # If that fails, try to find and use local dotenv file used during development - .env in your local dir
    load_dotenv()


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
    download_storage_dir = os.getenv('DOWNLOAD_STORAGE_DIR')
    if os.path.exists(download_storage_dir) and os.path.islink(download_storage_dir):
        storage_path = os.readlink(download_storage_dir)    # read the symlink

        if not os.path.exists(storage_path):                # symlink source doesn't exist? reset path to None
            storage_path = None

    if storage_path:    # if storage path was found, append subdir to it
        storage_path = os.path.join(storage_path, "fdd_imgs")
        os.makedirs(storage_path, exist_ok=True)

    # store the found storage path and return it
    last_storage_path = storage_path
    return storage_path


def send_to_socket(sock_path, item):
    """ send an item to core """
    try:
        app_log.debug(f"sending {item} to {sock_path}")
        json_item = json.dumps(item)   # dict to json

        sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
        sock.connect(sock_path)
        sock.send(json_item.encode('utf-8'))
        sock.close()
    except Exception as ex:
        app_log.debug(f"failed to send {item} - {str(ex)}")


def send_to_core(item):
    """ send an item to core """
    send_to_socket(os.getenv('CORE_SOCK_PATH'), item)


def send_to_taskq(item):
    """ send an item to task queue """
    send_to_socket(os.getenv('TASKQ_SOCK_PATH'), item)


def slot_insert(slot_index, path_to_image):
    """ insert floppy image to specified slot
    :param slot_index: index of slot to eject - 0-2
    :param path_to_image: filesystem path to image file which should be inserted
    """
    item = {'module': 'floppy', 'action': 'insert', 'slot': slot_index, 'image': path_to_image}
    send_to_core(item)


def slot_eject(slot_index):
    """ eject floppy image from specified slot
    :param slot_index: index of slot to eject - 0-2
    """
    item = {'module': 'floppy', 'action': 'eject', 'slot': slot_index}
    send_to_core(item)


def file_seems_to_be_image(path_to_image, check_if_exists):
    """ check if the supplied path seems to be image or not """
    if check_if_exists:
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
        return False, f"ext not supported: {ext}"

    # if we got here, it's a zip file
    try:
        with ZipFile(path_to_image, 'r') as zipObj:
            files = zipObj.namelist()       # Get list of files names in zip

            for file in files:              # go through all the files in zip
                success, message = file_seems_to_be_image(file, False)

                if success:
                    app_log.debug(f"the ZIP file {path_to_image} contains valid image {file}")
                    return True, None

    except Exception as ex:
        app_log.warning(f"file_seems_to_be_image failed: {str(ex)}")

    return False, "No valid image found in ZIP file"


def log_config():
    log_formatter = logging.Formatter('%(asctime)s %(levelname)s %(funcName)s(%(lineno)d) %(message)s')

    log_dir = os.getenv('LOG_DIR')
    os.makedirs(log_dir, exist_ok=True)
    log_file = os.path.join(log_dir, 'ce_fdd_py.log')

    os.makedirs(log_dir, exist_ok=True)
    my_handler = RotatingFileHandler(log_file, mode='a', maxBytes=1024 * 1024, backupCount=1)
    my_handler.setFormatter(log_formatter)
    my_handler.setLevel(logging.DEBUG)

    app_log = logging.getLogger()
    app_log.setLevel(logging.DEBUG)
    app_log.addHandler(my_handler)


def get_data_from_webserver(url_path, get_params=None):
    port = os.getenv('WEBSERVER_PORT')
    url = f"http://127.0.0.1:{port}/{url_path}"

    if get_params:                          # if get params were provided, add them to url
        query_string = urlencode(get_params)
        url = url + "?" + query_string

    response = urlopen(url)                 # store the response of URL
    data_json = json.loads(response.read()) # json string to dict
    return data_json


def get_list_of_lists():
    return get_data_from_webserver("download/list_of_lists")

