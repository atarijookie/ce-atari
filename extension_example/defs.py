# Fixed status bytes, used to signal status
STATUS_OK = 0
STATUS_NO_RESPONSE = 0xFF
STATUS_NO_SUCH_FUNCTION = 0xFE
STATUS_BAD_ARGUMENT = 0xFD
STATUS_EXT_NOT_RUNNING = 0xFC
STATUS_EXT_ERROR = 0xFB

# Supported data types in arguments and return values.
TYPE_NOT_PRESENT = 0            # when this type is not specified / used
TYPE_UINT8 = 1                  # uint8_t
TYPE_UINT16 = 2                 # uint16_t
TYPE_UINT32 = 3                 # uint32_t
TYPE_INT8 = 4                   # int8_t
TYPE_INT16 = 5                  # int16_t
TYPE_INT32 = 6                  # int32_t
TYPE_CSTRING = 7                # zero terminated string
TYPE_PATH = 8                   # file / dir path, as zero terminated string
TYPE_BIN_DATA = 9               # size + binary data buffer

# Supported response types
RESP_TYPE_NONE = 0              # no return value from function, will be turned into STATUS_OK and no data
RESP_TYPE_STATUS = 1            # return only status byte
RESP_TYPE_STATUS_BIN_DATA = 2   # return status byte and binary data buffer (could be also string)
RESP_TYPE_STATUS_PATH = 3       # return status byte and path, which needs transformation

MAX_FUNCTION_ARGUMENTS = 10     # 1 function can have up to this count of arguments
MAX_FUNCTION_NAME_LEN = 32      # maximum exported function length

MAX_EXPORTED_FUNCTIONS = 32     # maximum count of exported functions
