#ifndef EXTENSIONDEFS_H
#define EXTENSIONDEFS_H

#include <string>
#include <stdint.h>

#define MAX_EXTENSIONS_OPEN 3

#define EXT_BUFFER_SIZE     (257 * 512)

// Fixed status bytes, used to signal status
#define STATUS_OK               0
#define STATUS_NO_RESPONSE      0xFF
#define STATUS_NO_SUCH_FUNCTION 0xFE
#define STATUS_BAD_ARGUMENT     0xFD
#define STATUS_EXT_NOT_RUNNING  0xFC
#define STATUS_EXT_ERROR        0xFB

// Supported data types in arguments and return values.
#define TYPE_NOT_PRESENT        0       // when this type is not specified / used
#define TYPE_UINT8              1       // uint8_t
#define TYPE_UINT16             2       // uint16_t
#define TYPE_UINT32             3       // uint32_t
#define TYPE_INT8               4       // int8_t
#define TYPE_INT16              5       // int16_t
#define TYPE_INT32              6       // int32_t
#define TYPE_CSTRING            7       // zero terminated string
#define TYPE_PATH               8       // file / dir path, as zero terminated string
#define TYPE_BIN_DATA           9       // size + binary data buffer

#define MAX_FUNCTION_ARGUMENTS  10
#define MAX_FUNCTION_NAME_LEN   32

#define MAX_EXPORTED_FUNCTIONS  32

#define FAKE_INDEX_OPEN     (MAX_EXPORTED_FUNCTIONS + 1)
#define FAKE_INDEX_CLOSE    (FAKE_INDEX_OPEN + 1)

// The cmd[0] values that will be recognized as extension commands
#define CMD_CALL_RAW_READ           1
#define CMD_CALL_RAW_WRITE          2
#define CMD_CALL_LONG_WRITE_ARGS    5
#define CMD_CALL_GET_RESPONSE       6

// Basic function numbers for working with extensions - always the same, fixed values.
#define CEX_FUN_OPEN                0
#define CEX_FUN_GET_RESPONSE        1
#define CEX_FUN_CLOSE               2
// Additional (exported) functions from extension will have numbers 3 and higher.

/*
    Extension command structure:

    cmd[0] - ID + cmd_byte
    cmd[1] - extension id (handle from open())
    cmd[2] - function id (from function table)
    cmd[3] - count of sectors for this transfer
    cmd[4 - 5] - for arguments
*/
typedef struct 
{
    uint8_t idAndCmd;
    uint8_t extensionId;
    uint8_t functionId;
    uint8_t sectorCount;
    uint8_t arg1;
    uint8_t arg2;
} __attribute__((packed)) AcsiCommand;


typedef struct 
{
    char    name[MAX_FUNCTION_NAME_LEN];
    uint8_t argumentsCount;
    uint8_t argumentTypes[MAX_FUNCTION_ARGUMENTS];
    uint8_t returnValueType;
} __attribute__((packed)) ReceivedSignature;

typedef struct
{
    uint8_t index;
    uint8_t nameHash[2];
    uint8_t argumentTypes[MAX_FUNCTION_ARGUMENTS];
    uint8_t returnValueType;
} __attribute__((packed)) BinarySignatureForST;

#define MAX_BINARY_SIGNATURE_RESPONSE_SIZE  (MAX_EXPORTED_FUNCTIONS * sizeof(BinarySignatureForST))

#endif
