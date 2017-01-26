// vim: expandtab shiftwidth=4 tabstop=4 softtabstop=4
#include <string.h>
#include <stdio.h>

#include "scsi_defs.h"
#include "scsi.h"
#include "../global.h"
#include "../debug.h"
#include "devicemedia.h"
#include "imagefilemedia.h"

Scsi::Scsi(void)
{
    int i;

    dataTrans = 0;

    dataBuffer  = new BYTE[SCSI_BUFFER_SIZE];
    dataBuffer2 = new BYTE[SCSI_BUFFER_SIZE];

    for(i=0; i<8; i++) {
        devInfo[i].attachedMediaIndex   = -1;
        devInfo[i].accessType           = SCSI_ACCESSTYPE_NO_DATA;
    }

    for(i=0; i<MAX_ATTACHED_MEDIA; i++) {
        initializeAttachedMediaVars(i);
    }

    loadSettings();
}

Scsi::~Scsi()
{
    delete []dataBuffer;
    delete []dataBuffer2;

    for(int i=0; i<MAX_ATTACHED_MEDIA; i++) {                        // detach all attached media
        dettachByIndex(i);
    }
}

void Scsi::setAcsiDataTrans(AcsiDataTrans *dt)
{
    dataTrans = dt;
}

bool Scsi::attachToHostPath(std::string hostPath, int hostSourceType, int accessType)
{
    bool res;
    IMedia *dm;

    if(hostSourceType == SOURCETYPE_IMAGE_TRANSLATEDBOOT) {         // if we're trying to attach TRANSLATED boot image
        dettachBySourceType(SOURCETYPE_IMAGE_TRANSLATEDBOOT);       // first remove it, if we have it
    }

    int index = findAttachedMediaByHostPath(hostPath);              // check if we already don't have this

    if(index != -1) {                                               // if we already have this media
        if(attachedMedia[index].devInfoIndex == -1) {               // but it's not attached to ACSI ID
            res = attachMediaToACSIid(index, hostSourceType, accessType);          // attach media to ACSI ID

            if(res) {
                Debug::out(LOG_DEBUG, "Scsi::attachToHostPath - %s(%s) - media was already attached, attached to ACSI ID %d", hostPath.c_str(), SourceTypeStr(hostSourceType), attachedMedia[index].devInfoIndex);
            } else {
                Debug::out(LOG_DEBUG, "Scsi::attachToHostPath - %s(%s) - media was already attached, but still not attached to ACSI ID!", hostPath.c_str(), SourceTypeStr(hostSourceType));
            }

            return res;
        }

        // well, we have the media attached and we're also attached to ACSI ID
        Debug::out(LOG_DEBUG, "Scsi::attachToHostPath - %s(%s) - media was already attached to ACSI ID %d, not doing anything.", hostPath.c_str(), SourceTypeStr(hostSourceType), attachedMedia[index].devInfoIndex);
        return true;
    }

    index = findEmptyAttachSlot();                              // find where we can store it

    if(index == -1) {                                               // no more place to store it?
        Debug::out(LOG_ERROR, "Scsi::attachToHostPath - %s(%s) - no empty slot! Not attaching.", hostPath.c_str(), SourceTypeStr(hostSourceType));
        return false;
    }

    switch(hostSourceType) {                                                // try to open it depending on source type
    case SOURCETYPE_SD_CARD:
        attachedMedia[index].hostPath       = hostPath;
        attachedMedia[index].hostSourceType = SOURCETYPE_SD_CARD;
        attachedMedia[index].dataMedia      = &sdMedia;
        attachedMedia[index].accessType     = SCSI_ACCESSTYPE_FULL;
        attachedMedia[index].dataMediaDynamicallyAllocated = false;         // didn't use new on .dataMedia
        break;

    case SOURCETYPE_NONE:
        attachedMedia[index].hostPath       = hostPath;
        attachedMedia[index].hostSourceType = SOURCETYPE_NONE;
        attachedMedia[index].dataMedia      = &noMedia;
        attachedMedia[index].accessType     = SCSI_ACCESSTYPE_NO_DATA;
        attachedMedia[index].dataMediaDynamicallyAllocated = false;         // didn't use new on .dataMedia
        break;

    case SOURCETYPE_IMAGE:
        dm  = new ImageFileMedia();
        res = dm->iopen(hostPath.c_str(), false);                  // try to open the image

        if(res) {                                                           // image opened?
            attachedMedia[index].hostPath       = hostPath;
            attachedMedia[index].hostSourceType = hostSourceType;
            attachedMedia[index].dataMedia      = dm;

            if(hostSourceType != SOURCETYPE_IMAGE_TRANSLATEDBOOT) {                // for normal images - full access is allowed
                attachedMedia[index].accessType    = accessType;
            } else {                                                            // for translated boot image - read only
                attachedMedia[index].accessType    = SCSI_ACCESSTYPE_READ_ONLY;
            }

            attachedMedia[index].dataMediaDynamicallyAllocated = true;      // did use new on .dataMedia
        } else {                                                            // failed to open image?
            Debug::out(LOG_ERROR, "Scsi::attachToHostPath - failed to open image %s! Not attaching.", hostPath.c_str());
            attachedMedia[index].dataMediaDynamicallyAllocated = false;     // didn't use new on .dataMedia
            delete dm;
            return false;
        }
        break;

    case SOURCETYPE_IMAGE_TRANSLATEDBOOT:
        attachedMedia[index].hostPath       = hostPath;
        attachedMedia[index].hostSourceType = hostSourceType;
        attachedMedia[index].dataMedia      = &tranBootMedia;
        attachedMedia[index].accessType        = SCSI_ACCESSTYPE_READ_ONLY;
        attachedMedia[index].dataMediaDynamicallyAllocated = false;         // didn't use new on .dataMedia
        break;

    case SOURCETYPE_DEVICE:
        dm  = new DeviceMedia();
        res = dm->iopen(hostPath.c_str(), false);                   // try to open the device

        if(res) {
            attachedMedia[index].hostPath       = hostPath;
            attachedMedia[index].hostSourceType = hostSourceType;
            attachedMedia[index].dataMedia      = dm;
            attachedMedia[index].accessType     = SCSI_ACCESSTYPE_FULL;
            attachedMedia[index].dataMediaDynamicallyAllocated = true;      // did use new on .dataMedia
        } else {
            Debug::out(LOG_ERROR, "Scsi::attachToHostPath - failed to open device %s! Not attaching.", hostPath.c_str());
            attachedMedia[index].dataMediaDynamicallyAllocated = false;     // didn't use new on .dataMedia
            delete dm;
            return false;
        }

        break;

    case SOURCETYPE_TESTMEDIA:
        Debug::out(LOG_DEBUG, "Scsi::attachToHostPath - test media stored at index %d", index);

        attachedMedia[index].hostPath       = hostPath;
        attachedMedia[index].hostSourceType = hostSourceType;
        attachedMedia[index].dataMedia      = &testMedia;
        attachedMedia[index].accessType     = SCSI_ACCESSTYPE_FULL;
        attachedMedia[index].dataMediaDynamicallyAllocated = false;         // didn't use new on .dataMedia
        break;
    }

    res = attachMediaToACSIid(index, hostSourceType, accessType);          // last step - attach media to ACSI ID

    if(res) {
        Debug::out(LOG_DEBUG, "Scsi::attachToHostPath - %s - attached to ACSI ID %d", hostPath.c_str(), attachedMedia[index].devInfoIndex);
    } else {
        Debug::out(LOG_ERROR, "Scsi::attachToHostPath - %s - media attached, but not attached to ACSI ID!", hostPath.c_str());
    }

    return res;
}

bool Scsi::attachMediaToACSIid(int mediaIndex, int hostSourceType, int accessType)
{
    for(int i=0; i<8; i++) {                                                // find empty and proper ACSI ID
        // if this index is already used, skip it
        if(devInfo[i].attachedMediaIndex != -1) {
            continue;
        }
        bool canAttach;
        switch(acsiIdInfo.acsiIDdevType[i]) {
        case DEVTYPE_SD:    // only attaching SD CARDs
            canAttach = (hostSourceType == SOURCETYPE_SD_CARD);
            break;
        case DEVTYPE_RAW:   // attaching everything except SD CARDs and Translated boot
            canAttach = (hostSourceType != SOURCETYPE_SD_CARD) && (hostSourceType != SOURCETYPE_IMAGE_TRANSLATEDBOOT);
            break;
        case DEVTYPE_TRANSLATED:    // only attaching TRANSLATEDBOOT
            canAttach = (hostSourceType == SOURCETYPE_IMAGE_TRANSLATEDBOOT);
            break;
        default:
            canAttach = false;
        }
        if(canAttach) {
            devInfo[i].attachedMediaIndex   = mediaIndex;
            devInfo[i].accessType           = accessType;
            attachedMedia[mediaIndex].devInfoIndex = i;

            if(hostSourceType == SOURCETYPE_IMAGE_TRANSLATEDBOOT) {
                tranBootMedia.updateBootsectorConfigWithACSIid(i);        // and update boot sector config with ACSI ID to which this has been attached
            }
            return true;
        }
    }

    return false;
}

void Scsi::detachMediaFromACSIidByIndex(int index)
{
    if(index < 0 || index >= 8) {                                   // out of index?
        return;
    }

    if(devInfo[index].attachedMediaIndex == -1) {                   // nothing attached?
        return;
    }

    int attMediaInd = devInfo[index].attachedMediaIndex;
    if(    attachedMedia[attMediaInd].dataMediaDynamicallyAllocated) { // if dataMedia was creates using new, use delete
        if(attachedMedia[ attMediaInd ].dataMedia != NULL) {
            delete attachedMedia[ attMediaInd ].dataMedia;            // delete the data source access object
            attachedMedia[ attMediaInd ].dataMedia = NULL;
        }
    }
    initializeAttachedMediaVars(attMediaInd);

    devInfo[index].attachedMediaIndex   = -1;                       // set not attached in dev info
    devInfo[index].accessType           = SCSI_ACCESSTYPE_NO_DATA;
}

void Scsi::dettachFromHostPath(std::string hostPath)
{
    int index = findAttachedMediaByHostPath(hostPath);          // find media by host path

    if(index == -1) {                                           // not found? quit
        return;
    }

    dettachByIndex(index);                                      // found? detach!
}

void Scsi::detachAll(void)
{
    for(int i=0; i<8; i++) {
        detachMediaFromACSIidByIndex(i);
    }
}

void Scsi::detachAllUsbMedia(void)
{
    for(int i=0; i<8; i++) {                                                    // go through all the devices and detach only USB devices
        if(devInfo[i].attachedMediaIndex == -1) {                               // nothing attached?
            continue;
        }

        int attMediaInd = devInfo[i].attachedMediaIndex;
        if( attachedMedia[ attMediaInd ].hostSourceType != SOURCETYPE_DEVICE) { // this is not a USB device? skip it
            continue;
        }

        detachMediaFromACSIidByIndex(i);
    }
}

int Scsi::findAttachedMediaByHostPath(std::string hostPath)
{
    for(int i=0; i<MAX_ATTACHED_MEDIA; i++) {               // find where it's attached
        if(attachedMedia[i].hostPath == hostPath) {         // if found
            return i;
        }
    }

    return -1;                                              // if not found
}

void Scsi::dettachBySourceType(int hostSourceType)
{
    for(int i=0; i<MAX_ATTACHED_MEDIA; i++) {                       // find where it's attached
        if(attachedMedia[i].hostSourceType == hostSourceType) {     // if found
            dettachByIndex(i);
            return;
        }
    }
}

void Scsi::dettachByIndex(int index)
{
    if(index < 0 || index >= MAX_ATTACHED_MEDIA) {
        return;
    }

    if(attachedMedia[index].devInfoIndex != -1) {                           // if was attached to ACSI ID
        int ind2 = attachedMedia[index].devInfoIndex;

        detachMediaFromACSIidByIndex(ind2);
    }

    if(    attachedMedia[index].dataMediaDynamicallyAllocated) {               // if dataMedia was created using new, delete it
        if(attachedMedia[index].dataMedia != NULL) {
            attachedMedia[index].dataMedia->iclose();                       // close it, delete it
            delete attachedMedia[index].dataMedia;
            attachedMedia[index].dataMedia = NULL;
        }
    }

    initializeAttachedMediaVars(index);
}

void Scsi::initializeAttachedMediaVars(int index)
{
    if(index < 0 || index >= MAX_ATTACHED_MEDIA) {
        return;
    }

    attachedMedia[index].hostPath       = "";
    attachedMedia[index].hostSourceType = SOURCETYPE_NONE;
    attachedMedia[index].dataMedia      = NULL;
    attachedMedia[index].accessType     = SCSI_ACCESSTYPE_NO_DATA;
    attachedMedia[index].devInfoIndex   = -1;
    attachedMedia[index].dataMediaDynamicallyAllocated = false;
}

int Scsi::findEmptyAttachSlot(void)
{
    for(int i=0; i<MAX_ATTACHED_MEDIA; i++) {
        if(attachedMedia[i].dataMedia == NULL) {
            return i;
        }
    }

    return -1;
}

void Scsi::reloadSettings(int type)
{
    loadSettings();
}

void Scsi::loadSettings(void)
{
    Debug::out(LOG_DEBUG, "Scsi::loadSettings");

    // first read the new settings
    Settings s;
    s.loadAcsiIDs(&acsiIdInfo);

    // then dettach everything from ACSI IDs
    for(int i=0; i<8; i++) {
        detachMediaFromACSIidByIndex(i);
    }

    if(acsiIdInfo.sdCardAcsiId != 0xff) {                // if we got ACSI ID for SD card, attach this SD card...
        std::string sdCardHostPath("SD_CARD");
        attachToHostPath(sdCardHostPath, SOURCETYPE_SD_CARD, SCSI_ACCESSTYPE_FULL);
    }

    // attach Translated bootmedia
    attachToHostPath(TRANSLATEDBOOTMEDIA_FAKEPATH, SOURCETYPE_IMAGE_TRANSLATEDBOOT, SCSI_ACCESSTYPE_FULL);

    // TODO : attach RAW usb drives again
#if 0
    // and now reattach everything back according to new ACSI ID settings
    for(int i=0; i<MAX_ATTACHED_MEDIA; i++) {
        if(attachedMedia[i].dataMedia != NULL) {            // if there's some media to use
            attachMediaToACSIid(i, attachedMedia[i].hostSourceType, attachedMedia[i].accessType);
        }
    }
#endif

    std::string img;
    img = s.getString("HDDIMAGE", "");
    while(!img.empty() && (img[img.length()-1] == '\n' || img[img.length()-1] == ' '))
        img.erase(img.length()-1);
    if(!img.empty()) {
        int accessType = SCSI_ACCESSTYPE_FULL;
        size_t pos = img.find_last_of(' ');
        if(pos != std::string::npos) {
            std::string end = img.substr(pos+1);
            if(end.compare("READ_ONLY") == 0 || end.compare("READONLY") == 0 || end.compare("RO") == 0) {
                accessType = SCSI_ACCESSTYPE_READ_ONLY;
                img = img.substr(0, pos);
            }
        }
        if(!attachToHostPath(img, SOURCETYPE_IMAGE, accessType)) {
            Debug::out(LOG_ERROR, "Scsi::loadSettings fail to attach HDDIMAGE %s", img.c_str());
        }
    }
}

void Scsi::processCommand(BYTE *command)
{
    cmd     = command;
    acsiId  = cmd[0] >> 5;

    dataMedia = NULL;

    int attachedMediaIndex = devInfo[acsiId].attachedMediaIndex;

    if(attachedMediaIndex != -1) {                                  // if we got media attached to this ACSI ID
        dataMedia = attachedMedia[ attachedMediaIndex ].dataMedia;  // get pointer to dataMedia
    }

    if(dataTrans == 0) {
        Debug::out(LOG_ERROR, "Scsi::processCommand was called without valid dataTrans, can't tell ST that his went wrong...");
        return;
    }

    dataTrans->clear();                                         // clean data transporter before handling

    if(dataMedia == 0) {
        Debug::out(LOG_ERROR, "Scsi::processCommand was called without valid dataMedia, will return error CHECK CONDITION acsiId=%d attachedMediaIndex=%d", acsiId, attachedMediaIndex);
        dataTrans->setStatus(SCSI_ST_CHECK_CONDITION);
        dataTrans->sendDataAndStatus();
        return;
    }

    bool isIcd      = isICDcommand();                           // check if it's ICD command or SCSI(6) command
    BYTE lun        = isIcd ? (cmd[2] >> 5) : (cmd[1] >>   5);  // get LUN from command
    BYTE justCmd    = isIcd ? (cmd[1]     ) : (cmd[0] & 0x1f);  // get just the command (remove ACSI ID)

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
        dataTrans->sendDataAndStatus();                             // send all the stuff after handling, if we got any
    } else {                                                        // if data was already sent (large blocks in readSectors() or writeSectors()), we just need to send the status
        dataTrans->sendStatusToHans(devInfo[acsiId].LastStatus);
    }
}

void Scsi::ProcScsi6(BYTE lun, BYTE justCmd)
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
    BYTE res = 0;

    res = eraseMedia();
    //---------------
    if(res) {                                            // if everything was OK
        SendOKstatus();
    } else {                                        // if error
        storeSenseAndSendStatus(SCSI_ST_CHECK_CONDITION, SCSI_E_MediumError, SCSI_ASC_NO_ADDITIONAL_SENSE, SCSI_ASCQ_NO_ADDITIONAL_SENSE);
    }
}
//----------------------------------------------
void Scsi::SCSI_Inquiry(BYTE lun)
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

    if(devInfo[acsiId].attachedMediaIndex != -1) {
        int type = attachedMedia[devInfo[acsiId].attachedMediaIndex].hostSourceType;
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
    WORD length, i, len;
    BYTE PageCode, val;
    //-----------------
    BYTE page_control[]    = {0x0a, 0x06, 0, 0, 0, 0, 0, 0};
    BYTE page_medium[]    = {0x0b, 0x06, 0, 0, 0, 0, 0, 0};
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
void Scsi::SCSI_RequestSense(BYTE lun)
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
void Scsi::SendEmptySecotrs(WORD sectors)
{
    WORD i,j;

    for(j=0; j<sectors; j++)
    {
        for(i=0; i<512; i++)
        {
            dataTrans->addDataByte(0);
        }
    }
}
//----------------------------------------------
void Scsi::ProcICD(BYTE lun, BYTE justCmd)
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
    DWORD cap;
    BYTE hi,midlo, midhi, lo;

    int64_t scap, bcap;
    dataMedia->getCapacity(bcap, scap);

    cap = scap - 1;

    if(dataMedia->isInit()) {               // when initialized, store capacity
        hi        = (cap >> 24) & 0xff;
        midhi    = (cap >> 16) & 0xff;
        midlo    = (cap >>  8) & 0xff;
        lo        =  cap        & 0xff;
    } else {                                // when not initialized, store zeros
        hi        = 0;
        midhi    = 0;
        midlo    = 0;
        lo        = 0;
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
