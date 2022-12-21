import os
import json
import socket
import logging
from logging.handlers import RotatingFileHandler
from flask import render_template, request, current_app as app
from auth import login_required
from shared import PID_FILE, LOG_DIR, CORE_SOCK_PATH, FILE_FLOPPY_SLOTS


app_log = logging.getLogger()


def text_to_file(text, filename):
    # write text to file for later use
    try:
        with open(filename, 'wt') as f:
            f.write(text)
    except Exception as ex:
        app_log.warning(logging.WARNING, f"failed to write to {filename}: {str(ex)}")


def text_from_file(filename):
    # get text from file
    text = None

    if not os.path.exists(filename):    # no file like this exists? quit
        return None

    try:
        with open(filename, 'rt') as f:
            text = f.read()
            text = text.strip()         # remove whitespaces
    except Exception as ex:
        app_log.warning(logging.WARNING, f"failed to read {filename}: {str(ex)}")

    return text


def log_config():
    os.makedirs(LOG_DIR, exist_ok=True)
    log_formatter = logging.Formatter('%(asctime)s %(levelname)s %(funcName)s(%(lineno)d) %(message)s')

    my_handler = RotatingFileHandler(f'{LOG_DIR}/ce_webserver.log', mode='a', maxBytes=1024 * 1024, backupCount=1)
    my_handler.setFormatter(log_formatter)
    my_handler.setLevel(logging.DEBUG)

    app_log.setLevel(logging.DEBUG)
    app_log.addHandler(my_handler)


def other_instance_running():
    """ check if other instance of this app is running, return True if yes """
    pid_current = os.getpid()
    app_log.info(f'PID of this process: {pid_current}')

    os.makedirs(os.path.split(PID_FILE)[0], exist_ok=True)     # create dir for PID file if it doesn't exist

    # read PID from file and convert to int
    pid_from_file = -1
    try:
        pff = text_from_file(PID_FILE)
        pid_from_file = int(pff) if pff else -1
    except TypeError:       # we're expecting this on no text from file
        pass
    except Exception as ex:
        app_log.warning(f'other_instance_running: getting int PID from file failed: {type(ex).__name__} - {str(ex)}')

    # our and other PID match? no other instance
    if pid_current == pid_from_file:
        app_log.debug(f'other_instance_running: PID from file is ours, so other instance not running.')
        return False        # no other instance running

    # some other PID than ours was found in file
    if psutil.pid_exists(pid_from_file):
        app_log.warning(f'other_instance_running: Other mounter with PID {pid_from_file} is running!')
        return True         # other instance is running

    # other PID doesn't exist, no other instance running
    app_log.debug(f'other_instance_running: PID from file not running, so other instance not running')
    text_to_file(str(pid_current), PID_FILE)        # write our PID to file
    return False            # no other instance running


def template_renderer(app):
    def register_template_endpoint(name):
        @app.route('/' + name, endpoint=name)
        @login_required
        def route_handler():
            return render_template(name + '.html')
    return register_template_endpoint


def generate_routes_for_templates(app_in):
    app_in.logger.info("Generating routes for templates (for each worker)")

    web_dir = os.path.dirname(os.path.abspath(__file__))
    web_dir = os.path.join(web_dir, 'templates')
    app_in.logger.info(f'Will look for templates in dir: {web_dir}')
    register_template_endpoint = template_renderer(app_in)

    for filename in os.listdir(web_dir):                            # go through templates dir
        full_path = os.path.join(web_dir, filename)                 # construct full path

        fname_wo_ext, ext = os.path.splitext(filename)              # split to filename and extension

        # if it's not a file or doesn't end with htm / html, skip it
        if not os.path.isfile(full_path) or ext not in['.htm', '.html']:    # not a file or not supported extension?
            continue

        if fname_wo_ext == 'login':     # skip login page as we need this without @require_login
            continue

        app_in.logger.info(f'Added route /{fname_wo_ext} for template {filename}')
        register_template_endpoint(fname_wo_ext)


def get_arg_int(name, default=0):
    value = request.args.get(name, default)

    if value is None:       # don't even try to convert None, just return default
        return default

    try:
        value = int(value)  # try to convert to int, log exception
    except Exception as ex:
        app.logger.warning(f'failed to convert {value} to integer: {str(ex)}')
        return default

    return value


def send_to_core(item):
    """ send an item to core """
    try:
        app_log.debug(f"sending {item}")
        json_item = json.dumps(item)   # dict to json

        sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
        sock.connect(CORE_SOCK_PATH)
        sock.send(json_item.encode('utf-8'))
        sock.close()
    except Exception as ex:
        app_log.debug(f"failed to send {item} - {str(ex)}")


def slot_insert(slot_index, path_to_image):
    """ insert floppy image to specified slot
    :param slot_index: index of slot to insert - 0-2
    :param path_to_image: filesystem path to image file which should be inserted
    """
    item = {'module': 'floppy', 'action': 'insert', 'slot': slot_index, 'image': path_to_image}
    send_to_core(item)


def slot_activate(slot_index):
    """ activate floppy  slot
    :param slot_index: index of slot to activate - 0-2, or...
            100 - config image
            101 - test image
    """
    item = {'module': 'floppy', 'action': 'activate', 'slot': slot_index}
    send_to_core(item)


def slot_eject(slot_index):
    """ eject floppy image from specified slot
    :param slot_index: index of slot to eject - 0-2
    """
    item = {'module': 'floppy', 'action': 'eject', 'slot': slot_index}
    send_to_core(item)


def get_image_slots():
    # first get the image names that are in slot 1, 2, 3
    image_names = []
    txt_image_name = []

    try:
        with open(FILE_FLOPPY_SLOTS, 'rt') as f:
            txt_image_name = f.readlines()
    except Exception as ex:
        app_log.warning(f"Failed to open file {FILE_FLOPPY_SLOTS} : {str(ex)}")

    # now get images content for slot 1, 2, 3
    for i in range(3):
        if not i < len(txt_image_name):     # don't have this item at index i? quit
            break

        image_name = txt_image_name[i].strip()
        image_name = os.path.basename(image_name)           # get just filename from the path

        image_names.append(image_name)

    return image_names
