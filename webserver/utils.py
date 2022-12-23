import os
import json
import socket
import logging
from dotenv import load_dotenv
from zipfile import ZipFile
from logging.handlers import RotatingFileHandler
from flask import render_template, request, current_app as app
from auth import login_required
import shared


app_log = logging.getLogger()


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
    os.makedirs(os.getenv('LOG_DIR'), exist_ok=True)
    log_formatter = logging.Formatter('%(asctime)s %(levelname)s %(funcName)s(%(lineno)d) %(message)s')

    my_handler = RotatingFileHandler(os.path.join(os.getenv('LOG_DIR'), 'ce_webserver.log'),
                                     mode='a', maxBytes=1024 * 1024, backupCount=1)
    my_handler.setFormatter(log_formatter)
    my_handler.setLevel(logging.DEBUG)

    app_log.setLevel(logging.DEBUG)
    app_log.addHandler(my_handler)


def other_instance_running():
    """ check if other instance of this app is running, return True if yes """
    pid_current = os.getpid()
    app_log.info(f'PID of this process: {pid_current}')

    pid_file = os.path.join(os.getenv('PID_FILE'), 'ce_webserver.pid')
    os.makedirs(os.path.split(pid_file)[0], exist_ok=True)     # create dir for PID file if it doesn't exist

    # read PID from file and convert to int
    pid_from_file = -1
    try:
        pff = text_from_file(pid_file)
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
    text_to_file(str(pid_current), pid_file)        # write our PID to file
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
        with open(os.getenv('FILE_FLOPPY_SLOTS'), 'rt') as f:
            txt_image_name = f.readlines()
    except Exception as ex:
        app_log.warning(f"Failed to open file {os.getenv('FILE_FLOPPY_SLOTS')} : {str(ex)}")

    # now get images content for slot 1, 2, 3
    for i in range(3):
        if not i < len(txt_image_name):     # don't have this item at index i? quit
            break

        image_name = txt_image_name[i].strip()
        image_name = os.path.basename(image_name)           # get just filename from the path

        image_names.append(image_name)

    return image_names


def get_storage_path():
    """ Find the storage path and return it. Use cached value if possible. """

    # path still valid?
    last_storage_exists = shared.last_storage_path is not None and os.path.exists(shared.last_storage_path)

    if last_storage_exists:  # while the last storage exists, keep returning it
        return shared.last_storage_path

    storage_path = None

    # does this symlink exist?
    if os.path.exists(os.getenv('DOWNLOAD_STORAGE_DIR')) and os.path.islink(os.getenv('DOWNLOAD_STORAGE_DIR')):
        storage_path = os.readlink(os.getenv('DOWNLOAD_STORAGE_DIR'))    # read the symlink

        if not os.path.exists(storage_path):                # symlink source doesn't exist? reset path to None
            storage_path = None

    if storage_path:    # if storage path was found, append subdir to it
        storage_path = os.path.join(storage_path, "fdd_imgs")
        os.makedirs(storage_path, exist_ok=True)

    # store the found storage path and return it
    shared.last_storage_path = storage_path
    return storage_path


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
                    app.logger.debug(f"the ZIP file {path_to_image} contains valid image {file}")
                    return True, None

    except Exception as ex:
        app.logger.warning(f"file_seems_to_be_image failed: {str(ex)}")

    return False, "No valid image found in ZIP file"


def unlink_without_fail(path):
    # try to delete the symlink
    try:
        os.unlink(path)
        return True
    except FileNotFoundError:   # if it doesn't really exist, just ignore this exception (it's ok)
        pass
    except Exception as ex:     # if it existed (e.g. broken link) but failed to remove, log error
        app.logger.warning(f'failed to unlink {path} - exception: {str(ex)}')

    return False


def symlink_if_needed(source_path, symlink_path):
    """ This function creates symlink from mount_dir to symlink_dir, but tries to do it smart, e.g.:
        - checks if the source really exists, doesn't try if it doesn't
        - if the symlink doesn't exist, it will symlink it
        - if the symlink does exist, it checks where the link points, and when the link is what it should be, then it
          doesn't symlink anything, so it links only in cases where the link is wrong
    """

    # check if the source dir exists, fail if it doesn't
    if not os.path.exists(source_path):
        app.logger.warning(f"symlink_if_needed: source_path {source_path} does not exists!")
        return

    symlink_it = False

    if not os.path.exists(symlink_path):            # symlink dir doesn't exist - symlink it
        symlink_it = True
    else:                                           # symlink dir does exist - check if it's pointing to right source
        if os.path.islink(symlink_path):            # if it's a symlink
            source_dir = os.readlink(symlink_path)  # read symlink

            if source_dir != source_path:           # source of this link is not our mount dir
                symlink_it = True
        else:       # not a symlink - delete it, symlink it
            symlink_it = True

    # sym linking not needed? quit
    if not symlink_it:
        return

    # should symlink now
    unlink_without_fail(symlink_path)        # try to delete it

    try:
        os.symlink(source_path, symlink_path)  # symlink from mount path to symlink path
        app.logger.debug(f'symlink_if_needed: symlinked {source_path} -> {symlink_path}')
    except Exception as ex:
        app.logger.warning(f'symlink_if_needed: failed with: {type(ex).__name__} - {str(ex)}')
