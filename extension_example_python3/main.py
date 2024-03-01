#
# This is an CosmosEx extension example in python3.
# You can base your own extension based on this example, you just need to change
# EXTENSION_NAME and also define your exported functions in functions.py file.
# No other changes to this file are not needed.
#
# EXTENSION_NAME should match the directory name of this extension, so CE core will know
# where to look for start.sh and stop.sh scripts.
#
# Jookie, 2024
#

import socket
import os
import sys
import json
import traceback
import logging

from defs import *
from functions import *

EXTENSION_NAME = "extension_example"                # <<< CHANGE THIS IN YOUR OWN EXTENSION TO SOMETHING ELSE
IN_SOCKET_PATH = f"/tmp/{EXTENSION_NAME}.sock"      # where we will get commands from CE
out_socket_path = ""                                # where we should send responses to commands

should_run = True           # this extension should run while this flag is true
extension_id = -1           # CE knows this extension under this id (index), we must send it in responses

latest_data = bytearray()   # will hold received binary data and raw data


# Create socket on which this extension will receive commands / exported function calls
# from CE core. 
def create_socket():
    try:
        os.unlink(IN_SOCKET_PATH)
    except Exception as ex:
        if os.path.exists(IN_SOCKET_PATH):
            logging.warning(f"failed to unlink sock path: {IN_SOCKET_PATH} : {str(ex)}")
            raise

    sckt = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)

    try:
        sckt.bind(IN_SOCKET_PATH)
        sckt.settimeout(1.0)
        logging.debug(f'Success, got socket: {IN_SOCKET_PATH}')
        return sckt
    except Exception as e:
        logging.warning(f'exception on bind: {str(e)}')
        return False


# Send supplied data to CE core socket.
def send_to_ce(data):
    """ send an item to core """
    try:
        logging.debug(f"sending data to {out_socket_path} - {len(data)} bytes")
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
        sock.connect(out_socket_path)
        sock.send(data)
        sock.close()
    except Exception as ex:
        logging.warning(f"failed to send - {str(ex)}")


# convert supplied function name string to bytearray, padded to expected length
def function_name_to_bytearray(name):
    max_len = MAX_FUNCTION_NAME_LEN - 1

    if len(name) > max_len:
        logging.warning(f"Exported function name {name} is longer than {max_len}, this will not work. Make the name shorter!")
        exit(1)

    ret_val = bytearray(b'')
    name_bytes = name.encode('ascii')

    for i in range(0, MAX_FUNCTION_NAME_LEN):               # name to bytearray, pad with zeros up to max_len
        ret_val.append(name_bytes[i] if i < len(name_bytes) else 0)

    return ret_val


# This function turns the supplied function name, arguments and return value into byte array
# with the expected structure in the CE core. 
def function_signature_to_bytearray(fun_type, name, argumentTypes, returnValueType):
    if len(argumentTypes) > MAX_FUNCTION_ARGUMENTS:
        logging.warning(f"Exported function name {name} has {len(argumentTypes)} arguments, but only {MAX_FUNCTION_ARGUMENTS} are allowed. This will not work. Use less arguments!")
        exit(1)

    signature = function_name_to_bytearray(name)            # start with function name
    signature.append(fun_type)                              # type of function
    signature.append(len(argumentTypes))                    # add argumentsCount

    for i in range(0, MAX_FUNCTION_ARGUMENTS):              # add name to signature, pad with zeros up to max_len
        if i < len(argumentTypes):
            if argumentTypes[i] < TYPE_NOT_PRESENT or argumentTypes[i] > TYPE_BIN_DATA:     # invalid type?
                logging.warning(f"Invalid argument type {argumentTypes[i]}. This will not work. Use some of the TYPE_* values!")
                exit(1)

            signature.append(argumentTypes[i])              # append argument type
        else:
            signature.append(0)                             # append zeros

    if returnValueType < TYPE_NOT_PRESENT or returnValueType > TYPE_BIN_DATA:           # invalid type?
        logging.warning(f"Invalid return value type {returnValueType}. This will not work. Use some of the TYPE_* values!")
        exit(1)

    signature.append(returnValueType)                       # add return value type
    return signature


# Function to formulate response in expected format and send it to CE core.
def send_response(fun_name, status, data_out):
    data = bytearray()

    data.append(extension_id)                       # start with extension id
    data += function_name_to_bytearray(fun_name)    # this is a response on this function

    data += status.to_bytes(1, 'big')               # 1 B: status byte

    if data_out is not None:                        # get length if can
        if type(data_out) == str:                   # string to ascii bytes if needed, append terminating zero
            data_out = data_out.encode('ascii') + b'\0'

        data_len = len(data_out)
    else:                                           # use None if can't get length
        data_len = 0

    data += data_len.to_bytes(4, 'big')             # 4 B: data length

    if data_len > 0:
        data += data_out                            # data itself    

    send_to_ce(data)                                # send to CE now


def remove_trailing_zeros(data):
    while data[-1] == 0:        # while last char is zero, remove it
        data = data[:-1]

    return data


# This is a mandatory function which after the start of this extension will send signatures of exported functions
# to CE core, so the CE core will know that this extension has started and what function it exports.
#
# Currently the function signatures are defined manually, but this could be turned into automated function discovery.
# Done manually for simplicity and for simpler porting to other languages.
def send_exported_functions_table():
    data = bytearray()

    fns = get_exported_functions()

    for fun_name, fun_args_retval in fns.items():   # go through the exported functions and append their signatures to data
        fun_type, args, retval = fun_args_retval
        data += function_signature_to_bytearray(fun_type, fun_name, args, retval)

    data += IN_SOCKET_PATH.encode('ascii')          # add path to extension socket

    send_response("CEX_FUN_OPEN", len(fns), data)   # exported functions count instead of OK status


# Function used to send notification to CE core that this extension has closed.
def send_closed_notification():
    send_response("CEX_FUN_CLOSE", STATUS_OK, EXTENSION_NAME)       # send also extension name, so core won't accidentally mark other extension closed


# Function gets signature of single exported function identified by name.
def get_exported_function_signature(fun_name):
    fun_type, arg_types, retval_type = FUNC_RAW_WRITE, [], TYPE_NOT_PRESENT

    funs = get_exported_functions()             # get all exported functions
    signature = funs.get(fun_name, None)        # extract only the wanted function

    if signature is not None:                   # extraction found that function
        fun_type, arg_types, retval_type = signature

    return fun_type, arg_types, retval_type


# Handle RAW data from CE - sent when extension uses RAW call, or sends TYPE_BIN_DATA buffer.
# As we don't want to encode binary data to be sendable in JSON, we use first packet with
# raw binary data, which we store, and right after this packet the CE core will send
# one more packet with the JSON message, which will then use the data received here.
def handleRawData(data):
    data_len = int.from_bytes(data[2:6], byteorder ='big')      # get supplied data length

    # for now just store the data in global var for later usage
    global latest_data
    latest_data = data[6 : 6 + data_len]


# Handle JSON message from CE - used for calling exported functions (even the raw ones).
# RAW WRITE message will have raw_write in message set to something that evals as true, 
# then the previously raw binary data will be appended to arguments.
def handleJsonMessage(data):
    data = remove_trailing_zeros(data)              # JSON message must not contain trailing zero in data

    message = json.loads(data)                          # convert from json string to dictionary
    logging.debug(f'received message: {message}')

    fun_name = message['function']
    args = message.get('args', [])
    fun_type, arg_types, retval_type = get_exported_function_signature(fun_name)

    is_raw_write = fun_type == FUNC_RAW_WRITE   # True if this is raw write

    if is_raw_write:                        # if this is raw write, last argument should be latest binary data
        global latest_data
        args.append(latest_data)

    if fun_name == 'CEX_FUN_CLOSE':         # when this was a request to terminate this extension
        logging.debug("Received command from CE that we should terminate, so terminating now.")
        global should_run
        should_run = False
        return

    fun = globals().get(fun_name)

    if not fun:      # function not found? fail here
        logging.warning(f"Could not find function {fun_name}, message not handled")
        return

    expected_args_count = len(arg_types)        # how many arguments are expected
    supplied_args_count = len(args)             # how many arguments were actually supplied

    if expected_args_count > supplied_args_count:       # should have more args?
        missing_cnt = expected_args_count - supplied_args_count     # we're missing this many arguments
        for i in range(0, missing_cnt):                 # append missing args as zeros
            args.append(0)
    elif expected_args_count < supplied_args_count:     # should have less args? truncate args
        args = args[0 : expected_args_count]

    logging.debug(f"calling extension function {fun_name} with arguments: {args}")
    ret_val = fun(*args)                        # call the function

    # based on what was returned (nothing, integer, tuple, something else) try to split the return value into status and data
    if ret_val is None:                         # no return value from called function results in OK status with no data
        status_byte, raw_data = STATUS_OK, []
    elif type(ret_val) is tuple:                # if returning tuple, split it to status byte and data
        status_byte, raw_data = ret_val
    elif type(ret_val) is int:                  # if returning just integer, use it as status
        status_byte, raw_data = ret_val, []
    else:                                       # we don't know what to do with other options
        logging.warning(f"Function {fun_name} should return nothing, integer or tuple, but returned {type(ret_val)}")
        exit(1)

    if is_raw_write:                            # if this is a RAW WRITE, we always return just status (never data)
        retval_type = RESP_TYPE_STATUS

    if retval_type == RESP_TYPE_NONE:           # when there should be no response from function, always return OK
        status_byte = STATUS_OK

    # formulate response
    if retval_type in [RESP_TYPE_NONE, RESP_TYPE_STATUS]:   # should return only status byte, clear the data
        raw_data = None

    if type(raw_data) == str:                   # if it's a string, encode it from string to bytes
        raw_data = bytearray(raw_data.encode('ascii'))

        if raw_data[-1] != 0:           # if not zero terminated, zero terminate it now
            raw_data.append(0)

    send_response(fun_name, status_byte, raw_data)

#
# This is the main loop of the extension. It opens socket, receives the incoming packets
# and passes the received data to exported functions.
#
# expected arguments:
#   - path to CE core socket - where the responses from this extension will be sent
#   - id (index) of this extension - a number which will CE core use to identify from which extension the response came
#
# For running this with the test.py together, use the following command:
# python3 main.py /tmp/test.sock 2
#
if __name__ == "__main__":
    logging.basicConfig(filename='/tmp/extension_example.log', level=logging.DEBUG, format="%(asctime)s [%(levelname)s] %(message)s",)
    logging.getLogger().addHandler(logging.StreamHandler())
    logging.info('Started and log configured.')

    # path to CE sock must be supplied
    if(len(sys.argv)) < 3:
        logging.warning("path to CE socket and and extension ID not supplied, not starting!")
        exit(1)

    out_socket_path = sys.argv[1]
    extension_id = int(sys.argv[2])

    # try to create socket
    sock = create_socket()

    if not sock:
        logging.warning("Cannot run without socket! Terminating.")
        exit(1)

    logging.debug(f"Sending exported function signatures")
    send_exported_functions_table()

    logging.debug(f"Entering main loop, waiting for messages via: {IN_SOCKET_PATH}")

    # This receiving main loop with receive messages via UNIX domain sockets and submit them to thread pool executor.
    while should_run:
        try:
            data, address = sock.recvfrom(1024)         # receive message

            if len(data) < 2:
                logging.warning(f'received message too short, ignoring message')
                continue

            data2 = data[0:2]                           # get just first 2 bytes and convert them to string
            data2 = data2.decode('ascii')

            if data2[0] == 'D' and data2[1] == 'A':     # data starting with 'DA' - it's raw data
                handleRawData(data)
            elif data2[0] == '{':                       # data starting with '{' - it's JSON message
                handleJsonMessage(data)
            else:
                logging.warning(f'unknown message start, ignoring message')
                continue

        except socket.timeout:          # when socket fails to receive data
            pass
        except KeyboardInterrupt:
            logging.warning("Got keyboard interrupt, terminating.")
            break
        except Exception as ex:
            logging.warning(f"got exception: {str(ex)}")
            logging.warning(traceback.format_exc())

    # exited main loop and now terminating
    should_run = False
    send_closed_notification()
    os.unlink(IN_SOCKET_PATH)
