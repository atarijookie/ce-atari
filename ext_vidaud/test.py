import socket
import os
import sys
import json
from time import sleep
from defs import *

EXTENSION_NAME = "ext_vidaud"
IN_SOCKET_PATH = f"/tmp/{EXTENSION_NAME}.sock"

STATUS_NO_MORE_FRAMES = 0xF0

ext_socket_path = ""
extension_id = 2
TEST_SOCK_PATH = "/tmp/test.sock"

def remove_trailing_zeros(data):
    while data[-1] == 0:        # while last char is zero, remove it
        data = data[:-1]

    return data

def create_socket():
    try:
        os.unlink(TEST_SOCK_PATH)
    except Exception as ex:
        if os.path.exists(TEST_SOCK_PATH):
            print(f"failed to unlink sock path: {TEST_SOCK_PATH} : {str(ex)}")
            raise

    sckt = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)

    try:
        sckt.bind(TEST_SOCK_PATH)
        sckt.settimeout(1.0)
        print(f'Success, got socket: {TEST_SOCK_PATH}')
        return sckt
    except Exception as e:
        print(f'exception on bind: {str(e)}')
        return False


def send_to_ext(data):
    """ send an item to core """
    try:
        if type(data) == str:       # if it's a string, encode it to bytes
            data = data.encode('ascii')

        print(data)
        print(f"sending to {ext_socket_path}")
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
        sock.connect(ext_socket_path)
        sock.send(data)
        sock.close()
    except Exception as ex:
        print(f"failed to send - {str(ex)}")


# Find all the trailing zeros and remove them from the supplied string.
def trim_trailing_zeros(data):
    data_len = len(data)
    for i in range(data_len-1, 0, -1):      # go from end of string, find non-zero, trim string
        if data[i] != 0:
            data = data[0 : i+1]
            break

    return data


def verify_id_name(data, name):
    assert data[0] == extension_id, f"extension_id mismatch - {extension_id} != {data[0]}"

    name_in = data[1:33]
    name_in = trim_trailing_zeros(name_in)

    name_in = name_in.decode('ascii')
    assert name_in == name, f"name mismatch - '{name_in}' != '{name}'"

    status_in = data[33]
    return status_in


def verify_id_name_status_len(data, name, status, len):
    status_in = verify_id_name(data, name)
    assert status_in == status, f"status mismatch - {status_in} != {status}"

    data_rest_len = int.from_bytes(data[34:38], byteorder ='big')
    assert data_rest_len == len, f"rest of data length wrong - {data_rest_len} != {len}"

    return data[38:]        # returns raw response data part (== everything after header)


def get_data_from_sock(sock):
    while True:
        try:
            data, address = sock.recvfrom(1024)                 # receive message
            return data

        except socket.timeout:          # when socket fails to receive data
            pass
        except KeyboardInterrupt:
            print("Got keyboard interrupt, terminating.")
            return None
        except Exception as ex:
            print(f"got exception: {str(ex)}")


#
# This is a simple test for the this extension, used to test responses.
# It opens /tmp/test.sock socket and you must pass this path to extension so it will respond via this socket.
#
# For running this use the following command:
# python3 test.py
#
if __name__ == "__main__":
    print("Starting test")
    sock = create_socket()

    #---------
    print("Stoping and starting extension...")
    os.system("./stop.sh")                          # stop the extension if it's running

    sleep(5)

    start_command = f"./start.sh {TEST_SOCK_PATH} {extension_id} &"
    print("start command: ", start_command)
    os.system(start_command)                        # start the extension now

    #---------
    # should receive CEX_FUN_OPEN first, instead of status it returns count of exported functions, also returns path to socket
    response_raw = get_data_from_sock(sock)

    exported_func_count = 5
    func_signature_size = 45
    all_signatures_size = exported_func_count * func_signature_size

    resp_data = verify_id_name_status_len(response_raw, 'CEX_FUN_OPEN', exported_func_count, all_signatures_size + len(IN_SOCKET_PATH) + 1)
    print("CEX_FUN_OPEN - ok")

    ext_socket_path = resp_data[all_signatures_size:].decode('ascii')     # get extension's socket path - stored after all signatures
    print(f"extensions socket path: {ext_socket_path}")

    #---------
    # start the stream
    msg = {'function': 'start', 'args': [20, 3, 1, 25033, 1, "/tmp/ba.mp4"]}
    send_to_ext(json.dumps(msg))

    response_raw = get_data_from_sock(sock)
    resp_data = verify_id_name_status_len(response_raw, 'start', 3, 0)      # 3 means play video and audio
    print("start - ok")

    #--------
    for i in range(10):
        msg = {'function': 'get_frame_count', 'args': [0]}
        send_to_ext(json.dumps(msg))
        response_raw = get_data_from_sock(sock)
        status = verify_id_name(response_raw, 'get_frame_count')
        print(f"get_frame_count: {status}")

        if status > 10 and status < 100:     # got at least 10 frames, but not too many (would be error)
            break

        sleep(0.3)

    #---------
    # get the frames
    msg = {'function': 'get_frames', 'args': [4]}
    send_to_ext(json.dumps(msg))

    for i in range(25):
        print("get_frames", i)
        response_raw = get_data_from_sock(sock)
        status = verify_id_name(response_raw, 'get_frames')
        print("status", status)

    print("get frames - ok")

    response_raw = get_data_from_sock(sock)
    resp_data = verify_id_name_status_len(response_raw, 'get_frames', STATUS_NO_MORE_FRAMES, 0)

    #---------
    # stop the stream
    msg = {'function': 'stop', 'args': []}
    send_to_ext(json.dumps(msg))

    response_raw = get_data_from_sock(sock)
    resp_data = verify_id_name_status_len(response_raw, 'stop', STATUS_OK, 0)
    print("stop - ok")

    #---------
    # tell the extension to close
    msg = {'function': 'CEX_FUN_CLOSE', 'args': []}
    send_to_ext(json.dumps(msg))
    response_raw = get_data_from_sock(sock)
    resp_data = verify_id_name_status_len(response_raw, 'CEX_FUN_CLOSE', STATUS_OK, len(EXTENSION_NAME) + 1)
    ext_name = remove_trailing_zeros(resp_data).decode('ascii')
    assert ext_name == EXTENSION_NAME, f"received ext_name mismatch - '{type(ext_name)} {ext_name}' != '{EXTENSION_NAME} {type(EXTENSION_NAME)}'"

    #---------
    os.system("./stop.sh")                          # stop the extension if it's running

    sock.close()
    os.unlink(TEST_SOCK_PATH)
    print("Test finished.")
