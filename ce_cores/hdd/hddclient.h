#ifndef HDDCLIENT_H
#define HDDCLIENT_H

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
    HddClient(ChipInterface* cin, int& fdClient);
    virtual ~HddClient();

    bool handleHdd(uint8_t* inBuff);
    void reloadDrivesRaw(void);
    void reloadDrivesTranslated(void);

    void sendHalfWord(void);

private:
    uint8_t inBuff[INBUF_SIZE];
    uint8_t outBuf[INBUF_SIZE];

    ChipInterface* chipInterface;
    int& fdClient;

    THwConfig hwConfig;
    Scsi* scsi;
    TranslatedDisk* translated;
    AcsiDataTrans* dataTrans;

    //-----------------------------------
    // hard disk stuff
    AcsiIDinfo acsiIdInfo;
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
