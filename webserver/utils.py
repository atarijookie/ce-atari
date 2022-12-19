import os
import logging
from logging.handlers import RotatingFileHandler
from flask import render_template
from shared import PID_FILE, LOG_DIR


app_log = logging.getLogger()


def text_to_file(text, filename):
    # write text to file for later use
    try:
        with open(filename, 'wt') as f:
            f.write(text)
    except Exception as ex:
        app_log.warning(logging.WARNING, f"mount_shared: failed to write to {filename}: {str(ex)}")


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
        app_log.warning(logging.WARNING, f"mount_shared: failed to read {filename}: {str(ex)}")

    return text


def log_config():
    os.makedirs(LOG_DIR, exist_ok=True)
    log_formatter = logging.Formatter('%(asctime)s %(levelname)s %(funcName)s(%(lineno)d) %(message)s')

    my_handler = RotatingFileHandler(f'{LOG_DIR}/ce_webserver.log', mode='a', maxBytes=1024 * 1024, backupCount=1)
    my_handler.setFormatter(log_formatter)
    my_handler.setLevel(logging.DEBUG)

    app_log = logging.getLogger()
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
        def route_handler():
            return render_template(name + '.html')
    return register_template_endpoint


def generate_routes_for_templates(app):
    app.logger.info("Generating routes for templates (for each worker)")

    web_dir = os.path.dirname(os.path.abspath(__file__))
    web_dir = os.path.join(web_dir, 'templates')
    app.logger.info(f'Will look for templates in dir: {web_dir}')
    register_template_endpoint = template_renderer(app)

    for filename in os.listdir(web_dir):                            # go through templates dir
        full_path = os.path.join(web_dir, filename)                 # construct full path

        fname_wo_ext, ext = os.path.splitext(filename)              # split to filename and extension

        # if it's not a file or doesn't end with htm / html, skip it
        if not os.path.isfile(full_path) or ext not in['.htm', '.html']:    # not a file or not supported extension?
            continue

        app.logger.info(f'Added route /{fname_wo_ext} for template {filename}')
        register_template_endpoint(fname_wo_ext)
