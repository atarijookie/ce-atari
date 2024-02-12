import socket
import os
import sys
import json
from main import IN_SOCKET_PATH as ext_socket_path
from main import EXTENSION_NAME
from defs import *

extension_id = 2
TEST_SOCK_PATH = "/tmp/test.sock"

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


def verify_id_name_status_len(data, name, status, len):
    assert data[0] == extension_id, f"extension_id mismatch - {extension_id} != {data[0]}"

    name_in = data[1:33]
    name_in = trim_trailing_zeros(name_in)

    name_in = name_in.decode('ascii')
    assert name_in == name, f"name mismatch - '{name_in}' != '{name}'"

    status_in = data[33]
    assert status_in == status, f"status mismatch - {status_in} != {status}"

    data_rest_len = int.from_bytes(data[34:38], byteorder ='big')
    assert data_rest_len == len, f"rest of data length wrong - {data_rest_len} != {len}"


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
    data = get_data_from_sock(sock)
    verify_id_name_status_len(data, 'CEX_FUN_OPEN', 7, 7 * 44)        # should receive CEX_FUN_OPEN first, instead of status it returns count of exported functions
    print("CEX_FUN_OPEN - ok")

    #---------
    # call function without arguments
    msg = {'function': 'no_in_no_out', 'args': []}
    send_to_ext(json.dumps(msg))

    data = get_data_from_sock(sock)
    verify_id_name_status_len(data, 'no_in_no_out', STATUS_OK, 0)        # should receive no_in_no_out
    print("no_in_no_out - ok")

    #---------
    # call function with excessive arguments - will truncate excessive arguments
    msg = {'function': 'no_in_no_out', 'args': [1, 2]}
    send_to_ext(json.dumps(msg))
    data = get_data_from_sock(sock)
    verify_id_name_status_len(data, 'no_in_no_out', STATUS_OK, 0)        # should receive no_in_no_out
    print("no_in_no_out excessive - ok")

    #---------
    # call function to do the sum, return value in data
    msg = {'function': 'sum_of_two', 'args': [2, 3]}
    send_to_ext(json.dumps(msg))
    data = get_data_from_sock(sock)
    verify_id_name_status_len(data, 'sum_of_two', STATUS_OK, 4)

    sum = int.from_bytes(data[38:42], byteorder ='big')
    assert sum == 5, f"Wrong sum of 2+3 - {sum}"
    print("sum_of_two - ok")

    #---------
    # call function to do the sum with insufficient argument count - will fill the rest of args with zeros
    msg = {'function': 'sum_of_two', 'args': [7]}
    send_to_ext(json.dumps(msg))
    data = get_data_from_sock(sock)
    verify_id_name_status_len(data, 'sum_of_two', STATUS_OK, 4)

    sum = int.from_bytes(data[38:42], byteorder ='big')
    assert sum == 7, f"Wrong sum of 7+0 - {sum}"
    print("sum_of_two insufficient - ok")

    #---------
    # call function with string argument and string response
    msg = {'function': 'reverse_str', 'args': ['input string']}
    send_to_ext(json.dumps(msg))
    data = get_data_from_sock(sock)
    verify_id_name_status_len(data, 'reverse_str', STATUS_OK, 13)   # length is 13 because it counts also the terminating zero

    resp_str = data[38:]
    resp_str = trim_trailing_zeros(resp_str)
    resp_str = resp_str.decode('ascii')
    assert resp_str == 'gnirts tupni', f"Wrong response string - '{resp_str}' != 'gnirts tupni'"
    print("reverse_str - ok")

    #---------
    # call function with path argument and path response
    msg = {'function': 'fun_path_in', 'args': ['/tmp/bla.txt']}
    send_to_ext(json.dumps(msg))
    data = get_data_from_sock(sock)
    verify_id_name_status_len(data, 'fun_path_in', STATUS_OK, 13)   # length is 13 because it counts also the terminating zero

    resp_str = data[38:]
    resp_str = trim_trailing_zeros(resp_str)
    resp_str = resp_str.decode('ascii')
    assert resp_str == '/tmp/bla.txt', f"Wrong response string - '{resp_str}' != '/tmp/bla.txt'"
    print("fun_path_in - ok")

    #---------
    # call raw write function

    # first we create raw data packet
    raw_data = bytearray()      # this is the actual raw data from ST 
    for i in range(0, 5):       # 0 1 2 3 4 = sum is 10
        raw_data.append(i)

    raw_data_packet = bytearray()
    raw_data_packet += b'DA'                                # 2 B: 'DA'
    raw_data_packet += len(raw_data).to_bytes(4, 'big')     # 4 B: data length
    raw_data_packet += raw_data                             # rest: actual raw data
    send_to_ext(raw_data_packet)        # send raw data before calling raw data function handler

    msg = {'function': 'raw_data_in', 'args': [2, 5], 'raw_write': True}        # 2 args from cmd4 and cmd5, additional flag that this is raw_write
    send_to_ext(json.dumps(msg))
    data = get_data_from_sock(sock)
    verify_id_name_status_len(data, 'raw_data_in', 17, 0)   # no response data in RAW WRITE, returns sum of raw data and both args
    print("raw_data_in - ok")

    #---------
    # call raw read function
    msg = {'function': 'raw_data_out', 'args': [10, 69]}     # call raw_data_out and it should return buffer with 10x value 69
    send_to_ext(json.dumps(msg))
    data = get_data_from_sock(sock)
    verify_id_name_status_len(data, 'raw_data_out', STATUS_OK, 10)  # response length was specified in args[0]

    data_expected = bytearray()
    for i in range(0, 10):          # generate expected data
        data_expected.append(69)

    data_in = data[38:]             # get received data
    assert data_in == data_expected, f"received data mismatch - '{data_in}' != '{data_expected}'"
    print("raw_data_out - ok")

    #---------
    # tell the extension to close
    msg = {'function': 'CEX_FUN_CLOSE', 'args': []}
    send_to_ext(json.dumps(msg))
    data = get_data_from_sock(sock)
    verify_id_name_status_len(data, 'CEX_FUN_CLOSE', STATUS_OK, len(EXTENSION_NAME))
    ext_name = data[38:].decode('ascii')
    assert ext_name == EXTENSION_NAME, f"received ext_name mismatch - '{ext_name}' != '{EXTENSION_NAME}'"

    #---------
    sock.close()
    os.unlink(TEST_SOCK_PATH)
    print("Test finished.")
