import ctypes
import os
import stat
import mmap
from flask import Blueprint, current_app as app, abort, make_response
from utils import text_from_file, send_to_core, value_to_int

screencast = Blueprint('screencast', __name__)

try:
    librt = ctypes.CDLL('librt.so')
except:
    librt = ctypes.CDLL('librt.so.1')

librt.shm_open.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_short]
librt.shm_open.restype = ctypes.c_int


def shm_open(name):
    """ shm_open() to open the shared memory file """

    name = bytes(name, 'ascii')         # python string to bytes

    result = librt.shm_open(
        ctypes.c_char_p(name),
        ctypes.c_int(os.O_RDWR),
        ctypes.c_short(stat.S_IRUSR | stat.S_IWUSR)
    )

    if result == -1:
        raise RuntimeError(os.strerror(ctypes.get_errno()))

    return result


@screencast.route('/getscreen', methods=['GET'])
def getscreen():
    data_size = 1 + 32 + 32000      # resolution + palette + screen data
    fd = -1

    # open the shared memory file using shm_open()
    try:
        mem_name = os.getenv('SCREENCAST_MEMORY_NAME')
        fd = shm_open(mem_name)
    except Exception as ex:
        app.logger.warning(f"failed to open shared memory: {str(ex)}")

    if fd == -1:
        abort(400, 'shared memory access failed')

    # get actual screen data
    data = ''
    with mmap.mmap(fd, 0, access=mmap.ACCESS_READ) as mem:
        data = mem.read(data_size)

    # close shared memory file
    os.close(fd)

    # create response and return it
    resp = make_response(data)
    resp.mimetype = 'application/binary'
    resp.headers['Cache-Control'] = 'no-cache, no-store'
    resp.headers['Pragma'] = 'no-cache'
    resp.headers['Content-Length'] = data_size
    return resp


@screencast.route('/do_screenshot', methods=['POST'])
def do_screenshot():
    """ do single screenshot """
    send_to_core({'module': 'screencast', 'action': 'do_screenshot'})
    return {'status': 'ok'}


@screencast.route('/screenshot_vbl_enable', methods=['POST'])
def screenshot_vbl_enable():
    """ disable screenshot vbl """
    send_to_core({'module': 'screencast', 'action': 'screenshot_vbl_enable'})
    return {'status': 'ok'}


@screencast.route('/screenshot_vbl_disable', methods=['POST'])
def screenshot_vbl_disable():
    """ enable screenshot vbl """
    send_to_core({'module': 'screencast', 'action': 'screenshot_vbl_disable'})
    return {'status': 'ok'}


@screencast.route('/screenshot_query_vbl', methods=['GET'])
def screenshot_query_vbl():
    """ get screenshot vbl state """
    ss_vbl_enabled_file = os.getenv('SCREENSHOT_VBL_ENABLED_FILE')
    ss_vbl_enabled = text_from_file(ss_vbl_enabled_file)
    ss_vbl_enabled = value_to_int(ss_vbl_enabled)
    return {'screenShotVblEnabled': ss_vbl_enabled}
