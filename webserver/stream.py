import os
import socket
from flask import Blueprint, make_response, request, current_app as app

stream = Blueprint('stream', __name__)


def read_from_unix_sock(sock_path):
    if not os.path.exists(sock_path):
        app.logger.warning(f"No socket present at {sock_path}")
        return '', 400

    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.setblocking(0)

    try:
        sock.connect(sock_path)
    except socket.error as ex:
        app.logger.warning(f"Failed to connect to {sock_path}: {str(ex)}")
        return '', 400

    sock.settimeout(1.0)

    data = ''
    try:
        data = sock.recv(16384)
        data = data.decode('utf-8')
    except socket.timeout:
        # app.logger.debug(f"recv data timeout")
        pass

    sock.close()

    resp = make_response(data)
    resp.mimetype = "text/html"
    return resp


def write_to_unix_sock(sock_path, data):
    if not os.path.exists(sock_path):
        app.logger.warning(f"No socket present at {sock_path}")
        return '', 400

    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.setblocking(0)

    try:
        sock.connect(sock_path)
    except socket.error as ex:
        app.logger.warning(f"Failed to connect to {sock_path}: {str(ex)}")
        return '', 400

    sock.settimeout(1.0)
    sock.send(data.encode('utf-8'))
    sock.close()

    return ''


# GET config stream
@stream.route('/config', methods=['GET'])
def config_get():
    return read_from_unix_sock(os.getenv('SOCK_PATH_CONFIG'))


# POST config stream
@stream.route('/config', methods=['POST'])
def config_post():
    data_dict = request.get_json(force=True)
    data = data_dict.get('data')
    return write_to_unix_sock(os.getenv('SOCK_PATH_CONFIG') + '.wo', data)


# GET floppy stream
@stream.route('/floppy', methods=['GET'])
def floppy_get():
    return read_from_unix_sock(os.getenv('SOCK_PATH_FLOPPY'))


# POST floppy stream
@stream.route('/floppy', methods=['POST'])
def floppy_post():
    data_dict = request.get_json(force=True)
    data = data_dict.get('data')
    return write_to_unix_sock(os.getenv('SOCK_PATH_FLOPPY') + '.wo', data)


# GET terminal stream
@stream.route('/terminal', methods=['GET'])
def terminal_get():
    return read_from_unix_sock(os.getenv('SOCK_PATH_TERMINAL'))


# POST terminal stream
@stream.route('/terminal', methods=['POST'])
def terminal_post():
    data_dict = request.get_json(force=True)
    data = data_dict.get('data')
    return write_to_unix_sock(os.getenv('SOCK_PATH_TERMINAL') + '.wo', data)
