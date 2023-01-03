import os
import json
from flask import Blueprint, request, abort, send_file
from utils import send_to_core, text_from_file

debug = Blueprint('debug', __name__)


@debug.route('/loglevel', methods=['GET'])
def get_loglevel():
    loglevel_file = os.getenv('CORE_LOGLEVEL_FILE')     # retrieve filename from env and loglevel from file
    loglevel = text_from_file(loglevel_file)

    if not loglevel:            # no loglevel retrieved? default to INFO (1)
        loglevel = 1

    return {'loglevel': loglevel}


@debug.route('/loglevel', methods=['POST'])
def set_loglevel():
    data_dict = request.get_json(force=True)
    loglevel = data_dict.get('loglevel')

    try:
        loglevel = int(loglevel)
    except ValueError:                  # if not int, fail
        abort(400, 'conversion to int failed')

    if loglevel not in range(5):        # if not from 0 to 4, fail
        abort(400, f'invalid loglevel {loglevel}')

    item = {'module': 'all', 'action': 'set_loglevel', 'loglevel': loglevel}
    send_to_core(item)

    return {'status': 'ok'}


@debug.route('/logfile/<path:filename>', methods=['GET'])
def get_logfile(filename):
    """ fetch single logfile """
    log_dir = os.getenv('LOG_DIR')                  # retrieve log dir from env
    log_path = os.path.join(log_dir, filename)      # construct full path

    if not os.path.exists(log_path) or not os.path.isfile(log_path):    # requested file not found? fail
        abort(400, f"file {filename} not found or not a file!")

    return send_file(log_path)


@debug.route('/status', methods=['GET'])
def get_statusfile():
    """ fetch status file  """
    item = {'module': 'all', 'action': 'generate_status'}
    send_to_core(item)

    status_path = os.getenv('CORE_STATUS_FILE')         # retrieve log dir from env

    if not os.path.exists(status_path):         # requested file not found? fail
        abort(400, f"status file not found")

    status = text_from_file(status_path)        # get file content
    status = json.loads(status)                 # from json string to dict
    return {'status': status}
