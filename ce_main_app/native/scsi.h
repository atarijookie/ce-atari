#ifndef SCSI_H
#define SCSI_H

#include <string>

#include "../acsidatatrans.h"
#include "../settings.h"
#include "imedia.h"
#include "nomedia.h"
#include "testmedia.h"
#include "translatedbootmedia.h"
#include "sdmedia.h"

#include <stdint.h>
#include "../isettingsuser.h"

#define SCSI_ACCESSTYPE_FULL            0
#define SCSI_ACCESSTYPE_READ_ONLY       1
#define SCSI_ACCESSTYPE_NO_DATA         2

#define SOURCETYPE_NONE                 0
#define SOURCETYPE_IMAGE                1
#define SOURCETYPE_IMAGE_TRANSLATEDBOOT 2
#define SOURCETYPE_DEVICE               3
#define SOURCETYPE_SD_CARD              4
#define SOURCETYPE_TESTMEDIA            100

#define MAX_ATTACHED_MEDIA              9

#define TRANSLATEDBOOTMEDIA_FAKEPATH    "TRANSLATED BOOT MEDIA"

#define RW_ERROR_OK                 0
#define RW_ERROR_FAIL_TOO_BIG       1
#define RW_ERROR_FAIL_OUT_OF_RANGE  2
#define RW_ERROR_FAIL_RW_OPERATION  3
#define RW_ERROR_VERIFY_MISCOMPARE  4

// The following buffer size is based on maximum read / write size in all the data media objects.
#define SCSI_BUFFER_SIZE        (MAXIMUM_SECTOR_COUNT_LANGE * 512)
#define BUFFER_SIZE_SECTORS     MAXIMUM_SECTOR_COUNT_LANGE

typedef struct {
    bool        enabled;
    uint8_t     LastStatus;         // last returned SCSI status
    uint8_t     SCSI_ASC;           // additional sense code
    uint8_t     SCSI_ASCQ;          // additional sense code qualifier
    uint8_t     SCSI_SK;            // sense key

    int         hostSourceType;                 // type: image or device
    int         accessType;                     // access type: read only, read write, no data

    IMedia      *dataMedia;                     // pointer to the data provider object
    bool        dataMediaDynamicallyAllocated;  // set to true if dataMedia was created by new and delete should be used, otherwise set to false and delete won't be used
} TDevInfo;

class Scsi
{
public:
    Scsi(void);
    virtual ~Scsi();

    void setAcsiDataTrans(AcsiDataTrans *dt);
    void findAttachedDisks(void);
    void processCommand(uint8_t *command);
    void updateTranslatedBootMedia(void);

    TDevInfo* getDevInfo(int id) {
        if(id < 0 || id >= 8) return NULL;
        return &devInfo[id];
    }

    static const char * SourceTypeStr(int sourceType);

private:
    AcsiDataTrans   *dataTrans;

    uint8_t            acsiId;                 // current acsi ID for the command
    IMedia          *dataMedia;             // current data media valid for current ACSI ID

    NoMedia             noMedia;
    TestMedia           testMedia;
    TranslatedBootMedia tranBootMedia;
    SdMedia             sdMedia;

    uint8_t            *dataBuffer;
    uint8_t            *dataBuffer2;

    uint8_t            shitHasHappened;

    bool            sendDataAndStatus_notJustStatus;

    TDevInfo        devInfo[8];

    AcsiIDinfo      acsiIdInfo;

    uint8_t *cmd;

    bool isICDcommand(void);

    // for 6-byte long commands - from scsi6
    void ProcScsi6(uint8_t lun, uint8_t justCmd);

    void SCSI_RequestSense(uint8_t lun);
    void SCSI_FormatUnit(void);

    void SCSI_ReadWrite6(bool readNotWrite);

    void SCSI_Inquiry(uint8_t lun);
    void SCSI_ModeSense6(void);

    void SendOKstatus(void);
    void ReturnStatusAccordingToIsInit(void);
    void ReturnUnitAttention(void);
    void ClearTheUnitAttention(void);
    void returnInvalidCommand(void);

    void SendEmptySecotrs(uint16_t sectors);

    // for commands longer than 6 bytes - from scsiICD
    void ProcICD(uint8_t lun, uint8_t justCmd);

    void SCSI_ReadCapacity(void);
    void ICD7_to_SCSI6(void);
    void SCSI_ReadWrite10(bool readNotWrite);
    void SCSI_Verify(void);

    void showCommand(uint16_t id, uint16_t length, uint16_t errCode);

    void readWriteGeneric(bool readNotWrite, uint32_t startingSector, uint32_t sectorCount);
    void storeSenseAndSendStatus(uint8_t status, uint8_t senseKey, uint8_t additionalSenseCode, uint8_t ascq);

    bool readSectors_small  (uint32_t startSectorNo, uint32_t sectorCount);
    bool writeSectors_small (uint32_t startSectorNo, uint32_t sectorCount);

    bool readSectors_big    (uint32_t startSectorNo, uint32_t sectorCount);
    bool writeSectors_big   (uint32_t startSectorNo, uint32_t sectorCount);

    bool compareSectors (uint32_t startSectorNo, uint32_t sectorCount);
    bool eraseMedia(void);

    void clearDevInfo(int index);

    const char *getCommandName(uint8_t cmd);
};

#endif
