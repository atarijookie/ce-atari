// vim: expandtab shiftwidth=4 tabstop=4 softtabstop=4
#include <string.h>
#include <stdio.h>

#include "scsi_defs.h"
#include "scsi.h"
#include "../global.h"
#include "../debug.h"
#include "../utils.h"
#include "devicemedia.h"
#include "imagefilemedia.h"

Scsi::Scsi(void)
{
    int i;

    dataTrans = 0;

    dataBuffer  = new uint8_t[SCSI_BUFFER_SIZE];
    dataBuffer2 = new uint8_t[SCSI_BUFFER_SIZE];

    for(i=0; i<8; i++) {
        clearDevInfo(i, true);
    }

    findAttachedDisks();
}

Scsi::~Scsi()
{
    delete []dataBuffer;
    delete []dataBuffer2;
}

void Scsi::setAcsiDataTrans(AcsiDataTrans *dt)
{
    dataTrans = dt;
}

void Scsi::findAttachedDisks(void)
{
    Debug::out(LOG_DEBUG, "Scsi::findAttachedDisks() - starting");

    std::string pathRaw = Utils::dotEnvValue("MOUNT_DIR_RAW");    // where the raw disks are symlinked
    Utils::mergeHostPaths(pathRaw, "/X");           // add placeholder
    int len = pathRaw.length();

    Settings s;
    s.loadAcsiIDs(&acsiIdInfo);

    for(int i=0; i<8; i++) {                        // go through all the possible drives
        pathRaw[len - 1] = (char) ('0' + i);        // replace placeholder / drive character with the current drive char

        bool isBoot = (acsiIdInfo.ceddId == i);     // if this ID is used for CE_DD booting
        bool isFile = Utils::fileExists(pathRaw);   // if it's a file, then it's an image
        bool isDev = Utils::devExists(pathRaw);     // device is a device is a device is a device ;)

        Debug::out(LOG_DEBUG, "Scsi::findAttachedDisks() - ID %d - isBoot: %d, isFile: %d, isDev: %d", i, isBoot, isFile, isDev);

        if(!isFile && !isDev && !isBoot) {          // not a file, not a dev, not a CE_DD boot?
            clearDevInfo(i, false);
            continue;
        }

        // it's a file or dir
        devInfo[i].enabled = true;

        if(isFile) {        // file means image
            devInfo[i].hostSourceType = SOURCETYPE_IMAGE;
            devInfo[i].accessType = SCSI_ACCESSTYPE_FULL;

            ImageFileMedia *media = new ImageFileMedia();       // image access media
            media->iopen(pathRaw.c_str(), false);               // open it
            devInfo[i].dataMedia = media;                       // assign

            Debug::out(LOG_DEBUG, "Scsi::findAttachedDisks() - ID %d -> IMAGE", i);
        } else if(isDev) {  // device is device
            devInfo[i].hostSourceType = SOURCETYPE_DEVICE;
            devInfo[i].accessType = SCSI_ACCESSTYPE_FULL;

            DeviceMedia *media = new DeviceMedia();             // device access media
            media->iopen(pathRaw.c_str(), false);               // open it
            devInfo[i].dataMedia = media;                       // assign

            Debug::out(LOG_DEBUG, "Scsi::findAttachedDisks() - ID %d -> DEVICE", i);
        } else if(isBoot) { // boot media here
            devInfo[i].hostSourceType = SOURCETYPE_IMAGE_TRANSLATEDBOOT;
            devInfo[i].accessType = SCSI_ACCESSTYPE_READ_ONLY;
            devInfo[i].dataMedia = &tranBootMedia;
            Debug::out(LOG_DEBUG, "Scsi::findAttachedDisks() - ID %d -> TranslatedBoot", i);

            tranBootMedia.updateBootsectorConfigWithACSIid(i);        // update boot sector config with ACSI ID
        } else {            // we don't know
            devInfo[i].hostSourceType = SOURCETYPE_NONE;
            devInfo[i].accessType = SCSI_ACCESSTYPE_NO_DATA;
            Debug::out(LOG_WARNING, "Scsi::findAttachedDisks() -- unknown source type!");
        }
    }
}

void Scsi::clearDevInfo(int index, bool noDelete)
{
    if(index < 0 || index >= MAX_ATTACHED_MEDIA) {
        return;
    }

    devInfo[index].enabled = false;
    devInfo[index].hostSourceType = SOURCETYPE_NONE;
    devInfo[index].accessType = SCSI_ACCESSTYPE_NO_DATA;

    if(!noDelete && devInfo[index].dataMedia) {     // if we should delete if something is found and it's not null
        devInfo[index].dataMedia->iclose();         // close if it was open
        delete devInfo[index].dataMedia;            // delete it
    }

    devInfo[index].dataMedia = NULL;                // pointer to NULL
    devInfo[index].dataMediaDynamicallyAllocated = false;
}

void Scsi::processCommand(uint8_t *command)
{
    cmd     = command;
    acsiId  = cmd[0] >> 5;

    dataMedia = devInfo[acsiId].dataMedia;  // get pointer to dataMedia

    if(dataTrans == 0) {
        Debug::out(LOG_ERROR, "Scsi::processCommand was called without valid dataTrans, can't tell ST that his went wrong...");
        return;
    }

    dataTrans->clear();                                         // clean data transporter before handling

    if(dataMedia == 0) {
        Debug::out(LOG_ERROR, "Scsi::processCommand was called without valid dataMedia, will return error CHECK CONDITION acsiId=%d", acsiId);
        dataTrans->setStatus(SCSI_ST_CHECK_CONDITION);
        dataTrans->sendDataAndStatus();
        return;
    }

    bool isIcd      = isICDcommand();                           // check if it's ICD command or SCSI(6) command
    uint8_t lun        = isIcd ? (cmd[2] >> 5) : (cmd[1] >>   5);  // get LUN from command
    uint8_t justCmd    = isIcd ? (cmd[1]     ) : (cmd[0] & 0x1f);  // get just the command (remove ACSI ID)

    Debug::out(LOG_DEBUG, "Scsi::processCommand -- isIcd: %d, ACSI ID: %d, LUN: %d, CMD: 0x%02x - %s", isIcd, acsiId, lun, justCmd, getCommandName(justCmd));

    sendDataAndStatus_notJustStatus = true;                     // if this is set, let acsiDataTrans send data and status; if it's false then the data was already sent and we just need to send the status

    if(lun != 0) {      // if LUN is not zero, the command is invalid
        if(justCmd == SCSI_C_REQUEST_SENSE) {                   // special handling in REQUEST SENSE
            SCSI_RequestSense(lun);
        } else if(justCmd == SCSI_C_INQUIRY) {                  // special handling in INQUIRY
            SCSI_Inquiry(lun);
        } else {                                                // invalid command for non-zero LUN, fail
            storeSenseAndSendStatus(SCSI_ST_CHECK_CONDITION, SCSI_E_IllegalRequest, SCSI_ASC_LU_NOT_SUPPORTED, SCSI_ASCQ_NO_ADDITIONAL_SENSE);

            Debug::out(LOG_DEBUG, "Scsi::processCommand() - non-zero LUN, failing");
        }
    } else {            // if LUN is zero, we're fine
        if(isIcd) {                         // if it's a ICD command
            ProcICD     (lun, justCmd);
        } else {                            // if it's a normal command
            ProcScsi6   (lun, justCmd);
        }
    }

    if(sendDataAndStatus_notJustStatus) {                           // if this is set, let acsiDataTrans send data and status (like in most of the code)
        Debug::out(LOG_DEBUG, "Scsi::processCommand -- now the acsiDataTrans will handle transfer to Hans");
        dataTrans->sendDataAndStatus();                             // send all the stuff after handling, if we got any
    } else {                                                        // if data was already sent (large blocks in readSectors() or writeSectors()), we just need to send the status
        Debug::out(LOG_DEBUG, "Scsi::processCommand -- data was already sent to Hans, just send status (0x%02x) to Hans", devInfo[acsiId].LastStatus);
        dataTrans->sendStatusToHans(devInfo[acsiId].LastStatus);
    }
}

void Scsi::ProcScsi6(uint8_t lun, uint8_t justCmd)
{
    shitHasHappened = 0;

    //----------------
    // now to solve the not initialized device
    if(!dataMedia->isInit())
    {
        // for the next 3 commands the device is not ready
        if((justCmd == SCSI_C_FORMAT_UNIT) || (justCmd == SCSI_C_READ6) || (justCmd == SCSI_C_WRITE6))
        {
            ReturnStatusAccordingToIsInit();
            return;
        }
    }
    //----------------
    // if media changed, and the command is not INQUIRY and REQUEST SENSE
    if(dataMedia->mediaChanged())
    {
        if((justCmd != SCSI_C_INQUIRY) && (justCmd != SCSI_C_REQUEST_SENSE))
        {
            ReturnUnitAttention();
            return;
        }
    }
    //----------------
    // for read only devices - write and format not supported
    if(devInfo[acsiId].accessType == SCSI_ACCESSTYPE_READ_ONLY) {
        if(justCmd == SCSI_C_FORMAT_UNIT || justCmd == SCSI_C_WRITE6) {
            returnInvalidCommand();
            return;
        }
    }

    // for no data (fake) devices - even the read is not supported
    if(devInfo[acsiId].accessType == SCSI_ACCESSTYPE_NO_DATA) {
        if(justCmd == SCSI_C_FORMAT_UNIT || justCmd == SCSI_C_WRITE6 || justCmd == SCSI_C_READ6) {
            returnInvalidCommand();
            return;
        }
    }
    //----------------
    switch(justCmd)
    {
    case SCSI_C_SEND_DIAGNOSTIC:
    case SCSI_C_RESERVE:
    case SCSI_C_RELEASE:
    case SCSI_C_TEST_UNIT_READY:    ReturnStatusAccordingToIsInit();    break;

    case SCSI_C_MODE_SENSE6:        SCSI_ModeSense6();                  break;

    case SCSI_C_REQUEST_SENSE:      SCSI_RequestSense(lun);             break;
    case SCSI_C_INQUIRY:            SCSI_Inquiry(lun);                  break;

    case SCSI_C_FORMAT_UNIT:        SCSI_FormatUnit();                  break;
    case SCSI_C_READ6:              SCSI_ReadWrite6(true);              break;
    case SCSI_C_WRITE6:             SCSI_ReadWrite6(false);             break;

    default:                        returnInvalidCommand();             break;
    }
}
//----------------------------------------------
void Scsi::SCSI_FormatUnit(void)
{
    uint8_t res = 0;

    res = eraseMedia();
    //---------------
    if(res) {                                            // if everything was OK
        SendOKstatus();
    } else {                                        // if error
        storeSenseAndSendStatus(SCSI_ST_CHECK_CONDITION, SCSI_E_MediumError, SCSI_ASC_NO_ADDITIONAL_SENSE, SCSI_ASCQ_NO_ADDITIONAL_SENSE);
    }
}
//----------------------------------------------
void Scsi::SCSI_Inquiry(uint8_t lun)
{
    int inquiryLength;

    char type_str[5] = "    ";

    if(dataMedia->mediaChanged())                                   // this command clears the unit attention state
        ClearTheUnitAttention();

    if(cmd[1] & 0x01)                                               // EVPD bit is set? Request for vital data?
    {                                                               // vital data not suported
        storeSenseAndSendStatus(SCSI_ST_CHECK_CONDITION, SCSI_E_IllegalRequest, SCSO_ASC_INVALID_FIELD_IN_CDB, SCSI_ASCQ_NO_ADDITIONAL_SENSE);
        return;
    }

    int type = devInfo[acsiId].hostSourceType;

    switch(type) {
        case SOURCETYPE_IMAGE:
          memcpy(type_str, "IMG ", 4);
          break;
        case SOURCETYPE_IMAGE_TRANSLATEDBOOT:
          memcpy(type_str, "CEDD", 4);
          break;
        case SOURCETYPE_DEVICE:
          memcpy(type_str, "RAW ", 4);
          break;
        case SOURCETYPE_SD_CARD:
          memcpy(type_str, "SD  ", 4);
          break;
        default:
          snprintf(type_str, sizeof(type_str), "%4d", type);
    }
    //-----------
    inquiryLength = cmd[4];                             // how many bytes should be sent

    //                      0   0   0   0   0   0   0   0   001111111111222222222233333333334444 
    //                      0   1   2   3   4   5   6   7   890123456789012345678901234567890123 
    char inquiryData[45] = "\x00\x80\x02\x02\x27\x00\x00\x00JOOKIE  CosmosEx 0 SD   2.0001/08/17";

    inquiryData[ 0] = (lun == 0) ? 0 : 0x7f;            // for LUN 0 use 0, for other LUNs use peripheralQualifier = 0x03, deviceType = 0x1f (0x7f)
    inquiryData[25] = '0' + acsiId;                     // send ACSI ID # (0 .. 7)

    memcpy(inquiryData + 27, type_str,              4); // IMG / CEDD / RAW / SD
    memcpy(inquiryData + 32, VERSION_STRING_SHORT,  4); // version string
    memcpy(inquiryData + 36, DATE_STRING,           8); // date string

    if(inquiryLength <= 44) {                           // send less than whole buffer? just send all requested
        dataTrans->addDataBfr(inquiryData, inquiryLength, false);
    } else {                                            // send more than whole buffer?
        dataTrans->addDataBfr(inquiryData, 44, false);  // send whole buffer

        int i;
        for(i=0; i<(inquiryLength - 44); i++) {         // pad with zeros
            dataTrans->addDataByte(0);
        }
    }

    SendOKstatus();
}
//----------------------------------------------
void Scsi::SCSI_ModeSense6(void)
{
    uint16_t length, i, len;
    uint8_t PageCode, val;
    //-----------------
    uint8_t page_control[]    = {0x0a, 0x06, 0, 0, 0, 0, 0, 0};
    uint8_t page_medium[]    = {0x0b, 0x06, 0, 0, 0, 0, 0, 0};
    //-----------------
    PageCode    = cmd[2] & 0x3f;    // get only page code
    length        = cmd[4];                  // how many bytes should be sent

    //-----------------
    // page not supported?
    if(PageCode != 0x0a && PageCode != 0x0b && PageCode != 0x3f)
    {
        storeSenseAndSendStatus(SCSI_ST_CHECK_CONDITION, SCSI_E_IllegalRequest, SCSO_ASC_INVALID_FIELD_IN_CDB, SCSI_ASCQ_NO_ADDITIONAL_SENSE);
        return;
    }
    //-----------------
    // send the page

    switch(PageCode)
    {
    case 0x0a:
    case 0x0b:    len = 8;            break;
    case 0x3f:    len = 8 + 8;    break;
    default:        len = 0;            break;
    }

    for(i=0; i<length; i++)
    {
        val = 0;                                    // send 0 by default?

        if(i==0)                                    // Mode parameter header - Mode data length?
            val = 3 + len;

        if(PageCode == 0x0a)            // should send control page?
        {
            if(i>=4 && i<=11)
                val = page_control[i - 4];
        }

        if(PageCode == 0x0b)            // should send medium page?
        {
            if(i>=4 && i<=11)
                val = page_medium[i - 4];
        }

        if(PageCode == 0x3f)            // should send all pages?
        {
            if(i>=4 && i<=11)
                val = page_control[i - 4];

            if(i>=12 && i<=19)
                val = page_medium[i - 12];
        }

        dataTrans->addDataByte(val);
    }

    SendOKstatus();
}
//----------------------------------------------
// return the last error that occured
void Scsi::SCSI_RequestSense(uint8_t lun)
{
    char i,xx;
    unsigned char val;

    if(dataMedia->mediaChanged())       // this command clears the unit attention state
        ClearTheUnitAttention();

    xx = cmd[4];                        // how many bytes should be sent

    TDevInfo *device;
    TDevInfo badLunDevice;

    // pre-fill the bad device SC, ASC, ASCQ
    badLunDevice.SCSI_SK    = SCSI_E_IllegalRequest;
    badLunDevice.SCSI_ASC   = SCSI_ASC_LU_NOT_SUPPORTED;
    badLunDevice.SCSI_ASCQ = 0;

    // if LUN is zero, use device info, if LUN is non-zero, use bad device info
    device = (lun == 0) ? &devInfo[acsiId] : &badLunDevice;

    for(i=0; i<xx; i++)
    {
        switch(i)
        {
        case  0:    val = 0xf0;                 break;        // error code
        case  2:    val = device->SCSI_SK;      break;        // sense key
        case  7:    val = xx-7;                 break;        // AS length
        case 12:    val = device->SCSI_ASC;     break;        // additional sense code
        case 13:    val = device->SCSI_ASCQ;    break;        // additional sense code qualifier

        default:    val = 0;                    break;
        }

        dataTrans->addDataByte(val);
    }

    SendOKstatus();
}
//----------------------------------------------
void Scsi::SendEmptySecotrs(uint16_t sectors)
{
    uint16_t i,j;

    for(j=0; j<sectors; j++)
    {
        for(i=0; i<512; i++)
        {
            dataTrans->addDataByte(0);
        }
    }
}
//----------------------------------------------
void Scsi::ProcICD(uint8_t lun, uint8_t justCmd)
{
    shitHasHappened = 0;

    //----------------
    // now for the not present media
    if(!dataMedia->isInit())
    {
        // for the next 3 commands the device is not ready
        if((justCmd == SCSI_C_READ10) || (justCmd == SCSI_C_WRITE10) || (justCmd == SCSI_C_READ_CAPACITY))
        {
            ReturnStatusAccordingToIsInit();

            Debug::out(LOG_DEBUG, "Scsi::ProcICD - dataMedia is not init, returning status according to init");
            return;
        }
    }
    //----------------
    // if media changed, and the command is not INQUIRY and REQUEST SENSE
    if(dataMedia->mediaChanged())
    {
        if(justCmd != SCSI_C_INQUIRY)
        {
            ReturnUnitAttention();

            Debug::out(LOG_DEBUG, "Scsi::ProcICD - media changed, and this isn't INQUIRY command, returning UNIT ATTENTION");
            return;
        }
    }

    //----------------
    // for read only devices - write not supported
    if(devInfo[acsiId].accessType == SCSI_ACCESSTYPE_READ_ONLY) {
        if(justCmd == SCSI_C_WRITE10) {
            returnInvalidCommand();
            Debug::out(LOG_DEBUG, "Scsi::ProcICD - tried to WRITE on READ ONLY media, fail");
            return;
        }
    }

    // for no data (fake) devices - even the read is not supported
    if(devInfo[acsiId].accessType == SCSI_ACCESSTYPE_NO_DATA) {
        if(justCmd == SCSI_C_WRITE10 || justCmd == SCSI_C_READ10 || justCmd == SCSI_C_VERIFY) {
            returnInvalidCommand();
            Debug::out(LOG_DEBUG, "Scsi::ProcICD - READ / WRITE / VERIFY on empty media, fail");
            return;
        }
    }
    //----------------

    switch(justCmd)
    {
    case SCSI_C_READ_CAPACITY:
        SCSI_ReadCapacity();
        break;

    case SCSI_C_INQUIRY:
        ICD7_to_SCSI6();
        SCSI_Inquiry(lun);
        break;
        //------------------------------
    case SCSI_C_READ10:     SCSI_ReadWrite10(true);     break;
    case SCSI_C_WRITE10:    SCSI_ReadWrite10(false);    break;
    case SCSI_C_VERIFY:     SCSI_Verify();              break;
        //----------------------------------------------------
    default:
        returnInvalidCommand();
        break;
    }
}
//---------------------------------------------
void Scsi::SCSI_ReadCapacity(void)
{     // return disk capacity and sector size
    uint32_t cap;
    uint8_t hi,midlo, midhi, lo;

    int64_t scap, bcap;
    dataMedia->getCapacity(bcap, scap);

    cap = scap - 1;

    if(dataMedia->isInit()) {               // when initialized, store capacity
        hi      = (cap >> 24) & 0xff;
        midhi   = (cap >> 16) & 0xff;
        midlo   = (cap >>  8) & 0xff;
        lo      =  cap        & 0xff;
    } else {                                // when not initialized, store zeros
        hi      = 0;
        midhi   = 0;
        midlo   = 0;
        lo      = 0;
    }

    dataTrans->addDataByte(hi);                 // Hi
    dataTrans->addDataByte(midhi);          // mid-Hi
    dataTrans->addDataByte(midlo);          // mid-Lo
    dataTrans->addDataByte(lo);                 // Lo

    // return sector size
    dataTrans->addDataByte(0);                // fixed to 512 B
    dataTrans->addDataByte(0);
    dataTrans->addDataByte(2);
    dataTrans->addDataByte(0);

    SendOKstatus();
}
//---------------------------------------------
bool Scsi::eraseMedia(void)
{
    bool res;

    memset(dataBuffer, 0, 512);                                 // create empty buffer

    for(int i=0; i<100; i++) {                                  // write empty sector to position 0 .. 99
        res = dataMedia->writeSectors(i, 1, dataBuffer);

        if(!res) {                                              // failed to write?
            return false;
        }
    }

    return true;                                                // all good
}
//----------------------------------------------

void Scsi::updateTranslatedBootMedia(void)
{
    int i, idOnBus = -1;

    for(i=0; i<8; i++) {        // find where the CE_DD is on ACSI / SCSI bus
        if(acsiIdInfo.acsiIDdevType[i] == DEVTYPE_TRANSLATED) {
            idOnBus = i;
            break;
        }
    }

    if(idOnBus == -1) {         // don't have this media?
        Debug::out(LOG_DEBUG, "Scsi::updateTranslatedBootMedia() - could not find CE_DD bus ID, fail");
        return;
    } else {
        Debug::out(LOG_DEBUG, "Scsi::updateTranslatedBootMedia() - the bus ID of CE_DD is: %d", idOnBus);
    }

    tranBootMedia.updateBootsectorConfigWithACSIid(idOnBus);
}
