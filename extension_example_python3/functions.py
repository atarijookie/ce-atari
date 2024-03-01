import os
import sys
from defs import *

# Place all your exported functions here. 

# Place the functions which should be exported in this function. 
# Return value is a dictionary, where:
#   - key is exported function name
#   - value is a tupple with:
#       - function call type (where the args are, also read / write direction when calling)
#       - argument types list (what argument will be stored and later retrieved from the buffer / cmd[4] cmd[5])
#       - return value type
def get_exported_functions():
    fns = {}

    fns["no_in_no_out"] = FUNC_RAW_WRITE, [], RESP_TYPE_NONE                                        # this function has no arguments, returns nothing
    fns["one_in_one_out"] = FUNC_RAW_WRITE, [TYPE_UINT8], RESP_TYPE_STATUS                          # function expects 1 uint8 argument, returns uint8
    fns["sum_of_two"] = FUNC_LONG_ARGS, [TYPE_INT16, TYPE_INT16], RESP_TYPE_STATUS_BIN_DATA         # 2 arguments in, int16 out
    fns["reverse_str"] = FUNC_LONG_ARGS, [TYPE_CSTRING], RESP_TYPE_STATUS_BIN_DATA                  # string as argument, string as return value
    fns["fun_path_in"] = FUNC_LONG_ARGS, [TYPE_PATH], RESP_TYPE_STATUS_PATH                         # string path in, string as return value
    fns["raw_data_in"] = FUNC_RAW_WRITE, [TYPE_UINT8, TYPE_UINT8, TYPE_BIN_DATA], RESP_TYPE_STATUS  # raw data in can have 2 input bytes (from cmd4 and cmd5) and bin data, returns just status
    fns["raw_data_out"] = FUNC_RAW_READ, [TYPE_UINT8, TYPE_UINT8], RESP_TYPE_STATUS_BIN_DATA        # raw data out can have 2 input bytes (from cmd4 and cmd5), returns bin data and status

    return fns

# function with no arguments in and no return value
def no_in_no_out():
    print("no_in_no_out was called")
    # intentionaly no return value


# function with 1 in argument, returned back as status byte
def one_in_one_out(arg1):
    print(f"one_in_one_out was called with {arg1}")
    return arg1


# Add two numbers together, store the result in buffer
def sum_of_two(arg1, arg2):
    sum = arg1 + arg2
    ret = bytearray()
    ret += sum.to_bytes(4, 'big')       # sum of numbers to big endian bytes

    print(f"sum_of_two was called with {arg1} and {arg2}, will return {sum}")
    return STATUS_OK, ret


# String going in, being reversed and returned as binary data buffer
def reverse_str(arg1):
    ret = arg1[::-1]
    print(f"reverse_str was called with '{arg1}', will return '{ret}'")
    return STATUS_OK, ret


# Path supplied will be returned as path again.
def fun_path_in(arg1):
    print(f"fun_path_in was called with '{arg1}', will return the same")
    return STATUS_OK, arg1


# Function called for raw data input. 
# Function arguments ARE FIXED - don't change their count. 
# RAW WRITE always sends:
#   args: [TYPE_UINT8, TYPE_UINT8, TYPE_BIN_DATA]
#   response: RESP_TYPE_STATUS
def raw_data_in(cmd4, cmd5, raw_data):
    sum = cmd4 + cmd5

    for i in range(0, len(raw_data)):
        sum += raw_data[i]

    return sum


# Function called for raw data output - will add cmd4 count of bytes to response with value specified in cmd5
# Function arguments ARE FIXED - don't change their count. 
# RAW WRITE always sends:
#   args: [TYPE_UINT8, TYPE_UINT8]
#   response: RESP_TYPE_STATUS_BIN_DATA
def raw_data_out(cmd4, cmd5):

    resp = bytearray()

    for i in range(0, cmd4):        # generate cmd4 count of bytes with value stored in cmd5
        resp.append(cmd5)

    return STATUS_OK, resp

