from flask import Blueprint, request, current_app as app, abort
from utils import send_to_core

hid = Blueprint('hid', __name__)


@hid.route('/mouse', methods=['POST'])
def mouse_post():
    data_dict = request.get_json(force=True)

    item = {'module': 'ikbd', 'action': 'mouse'}    # start the message in expected format
    item.update(data_dict)      # add received data
    send_to_core(item)          # send to core

    return {'status': 'ok'}


@hid.route('/keyboard', methods=['POST'])
def keyboard_post():
    data_dict = request.get_json(force=True)

    item = {'module': 'ikbd', 'action': 'keyboard'}    # start the message in expected format
    item.update(data_dict)      # add received data
    send_to_core(item)          # send to core

    return {'status': 'ok'}
