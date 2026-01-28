#ifndef HDDCLIENT_H
#define HDDCLIENT_H

#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <unistd.h>

#include "../misc/global.h"
#include "../misc/settings.h"
#include "../misc/version.h"
#include "../misc/utils.h"
#include "../chipinterface/chipinterface.h"

class AcsiDataTrans;
class RetryModule;
class ChipInterfaceClient;
class ChipInterface;
class TranslatedDisk;
class Scsi;

class HddClient
{
public:
    HddClient(ChipInterface* cin, ClientInfo* ci, int aSharedMemFd, sem_t* aSharedMemSemaphore, uint8_t* aSharedMemPointer);
    virtual ~HddClient();

    bool handleHdd(uint8_t* inBuff);
    void reloadDrivesRaw(void);
    void reloadDrivesTranslated(void);

    void sendHalfWord(void);

    void onMacUpdated(void);

private:
    uint8_t inBuff[INBUF_SIZE];
    uint8_t outBuf[INBUF_SIZE];

    ChipInterface* chipInterface;
    ClientInfo* ci;

    THwConfig hwConfig;
    Scsi* scsi;
    TranslatedDisk* translated;
    AcsiDataTrans* dataTrans;

    // for screencast
    int sharedMemFd;
    sem_t* sharedMemSemaphore;
    uint8_t *sharedMemPointer;

    //-----------------------------------
    // hard disk stuff
    RetryModule* retryMod;

    void handleAcsiCommand(uint8_t *bufIn);

    //-----------------------------------
    // handle FW version
    void handleFwVersion_hans(void);

    void saveHwConfig(void);
    uint8_t getIdBits(void);

    //----------------------------------
    // other
    void extractInterfaceInfo(uint8_t xilinxInfo);
};

#endif
