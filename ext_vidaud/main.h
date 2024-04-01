#ifndef __MAIN_H__
#define __MAIN_H__

#include <stdint.h>
#include "extensiondefs.h"

#define EXTENSION_NAME      "ext_vidaud"
#define IN_SOCKET_PATH      "/tmp/" EXTENSION_NAME ".sock"  // where we will get commands from CE
#define LOG_FILE_NAME       "/tmp/" EXTENSION_NAME ".log"

int createRecvSocket(const char* pathToSocket);
void addFunctionSignature(void* pFunc, const char* name, uint8_t fun_type, uint8_t* argumentTypes, uint8_t argumentTypesCount, uint8_t returnValueType);
void functionSignatureToBytes(const char* name, uint8_t fun_type, uint8_t* argumentTypes, uint8_t argumentTypesCount, uint8_t returnValueType, ReceivedSignature* signature);
void handleJsonMessage(uint8_t* bfr);
void handleRawData(uint8_t* data);
uint16_t getWord(uint8_t *bfr);
uint32_t getDword(uint8_t *bfr);
void storeWord(uint8_t *bfr, uint16_t val);
void storeDword(uint8_t *bfr, uint32_t val);
void sendExportedFunctionsTable(void);
void sendClosedNotification(void);
void responseInit(ResponseFromExtension* resp, const char* funName);
void responseStoreStatusAndDataLen(ResponseFromExtension* resp, uint8_t status, uint32_t dataLen);
void responseStoreDataLen(ResponseFromExtension* resp, uint32_t dataLen);
ReceivedSignature* getExportedFunctionSignature(const char* funName);

#endif
