#ifndef __EXTENSION_H__
#define __EXTENSION_H__

#include <stdint.h>
#include "extensiondefs.h"

#define DATA_DIR_WRITE  0
#define DATA_DIR_READ   1

typedef struct 
{
    uint8_t readNotWrite;   // non-zero for read operation, zero for write operation
    uint8_t cmd;            // ID will be added in CE_IDD
    uint8_t extensionId;    // handle of the extension returned when opening extension
    uint8_t functionId;     // function id we want to call
    uint8_t sectorCount;    // how many data should be transfered
    uint8_t arg1;           // argument 1 on raw call
    uint8_t arg2;           // argument 2 on raw call
    uint8_t* buffer;        // where the data should be sent from or stored to
    uint8_t  statusByte;    // status byte from the call will be stored here
} __attribute__((packed)) CEXcall;

void cexDescribe(char* functionName);       // describe the function - args, data, return value (extension must be open for this)
uint8_t cexOpen(char* extensionName, char* extensionSourceUrl, uint16_t timeoutSeconds);
void cexClose(uint8_t extensionId);

uint8_t cexCallRawWrite(uint8_t extensionId, char* functionName, uint8_t arg1, uint8_t arg2, uint32_t dataLenBytes, uint8_t* pData);
uint8_t cexCallRawRead(uint8_t extensionId, char* functionName, uint8_t arg1, uint8_t arg2, uint32_t dataLenBytes, uint8_t* pData);
uint8_t cexCallLong(uint8_t extensionId, char* functionName, int argc, ...);
uint8_t cexResponse(uint8_t extensionId, char* functionName, uint32_t expectRespLenBytes, uint8_t* pResponse);

// ------------------------------------------------------------------------------------------------------------------------------
// Internal helper functions follow. Developer of plugin doesn't need to call this directly, use the API functions above instead.

uint16_t __calcHash(char* name);
BinarySignatureForST* __getSignatureByName(char* functionName);
BinarySignatureForST* __validate(char* functionName, uint8_t functionType, uint8_t argsCount);
const char* __getArgumentTypeString(uint8_t argType);
const char* __getResponseTypeString(uint8_t responseType);
uint8_t __bytesLenToSectorsLen(uint32_t dataLenBytes);
uint8_t* __getBuffer(void);
uint8_t __doTheCall(CEXcall* cc);

#define TAG_CE      0x4345
#define RET_CEDD    0x43454444

#define TAG_CX      0x4358
#define RET_CEXT    0x43455854

#endif
