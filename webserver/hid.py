from flask import Blueprint, request, current_app as app, abort

hid = Blueprint('hid', __name__)


@hid.route('/mouse', methods=['POST'])
def mouse_post():
    data_dict = request.get_json(force=True)
    return {'status': 'ok'}


@hid.route('/keyboard', methods=['POST'])
def keyboard_post():
    data_dict = request.get_json(force=True)
    return {'status': 'ok'}
