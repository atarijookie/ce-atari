#ifndef EXTENSIONHANDLER_H
#define EXTENSIONHANDLER_H

#include <string>
#include <stdint.h>

#include "../acsidatatrans.h"
#include "../settings.h"
#include "extension.h"

class ExtensionHandler
{
public:
    ExtensionHandler(void);
    virtual ~ExtensionHandler();

    bool isExtensionCall(uint8_t justCmd);
    void setAcsiDataTrans(AcsiDataTrans *dt);
    void processCommand(uint8_t *command);

    static void mutexLock(void);
    static void mutexUnlock(void);

    static void waitForSignal(uint32_t ms);
    static void setSignal(void);

private:
    AcsiDataTrans *dataTrans;

    uint8_t cmd4;
    uint8_t cmd5;

    uint32_t byteCountInDataBuffer;
    uint8_t *dataBuffer;
    uint8_t *dataBuffer2;

    const char *getCommandName(uint8_t cmd);

    void cexOpen(void);
    void cexStatusOrResponse(uint8_t extensionId, uint8_t funcCallId, uint8_t sectorCount);
    void cexClose(uint8_t extensionId);
    void cexExtensionFunction(uint8_t justCmd, uint8_t extensionId, uint8_t functionId, uint32_t sectorCount);

    uint8_t sendCallRawToExtension(uint8_t extensionId, char* functionName);
    uint8_t sendCallLongToExtension(uint8_t extensionId, uint8_t functionIndex, char* functionName);

    bool sendStringToExtension(uint8_t extensionId, const char* str);
    bool sendDataToExtension(uint8_t extensionId, const uint8_t* data, uint32_t dataLen);
    bool sendDataToSocket(const char* socketPath, const uint8_t* data, uint32_t dataLen);

    void getExtensionHandlerSockPath(std::string& sockPath);
    char* intToStr(int arg, char* bfr);
};

void *extensionThreadCode(void *ptr);

#endif
