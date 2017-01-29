// vim: expandtab shiftwidth=4 tabstop=4 softtabstop=4
#include <string.h>
#include <stdio.h>

#include "scsi_defs.h"
#include "scsi.h"
#include "../global.h"
#include "../debug.h"
#include "devicemedia.h"
#include "imagefilemedia.h"

//---------------------------------------------
void Scsi::SCSI_ReadWrite6(bool readNotWrite)
{
    DWORD startingSector;
    WORD sectorCount;

    startingSector  = cmd[1] & 0x1f;
    startingSector  = startingSector << 8;
    startingSector |= cmd[2];
    startingSector  = startingSector << 8;
    startingSector |= cmd[3];

    sectorCount     = cmd[4];                   // get the # of sectors to read

    if(sectorCount == 0) {
        sectorCount = 256;
    }

    readWriteGeneric(readNotWrite, startingSector, sectorCount);
}

//---------------------------------------------
void Scsi::SCSI_ReadWrite10(bool readNotWrite)
{
    DWORD startingSector;
    WORD  sectorCount;

    startingSector  = cmd[3];
    startingSector  = startingSector << 8;
    startingSector |= cmd[4];
    startingSector  = startingSector << 8;
    startingSector |= cmd[5];
    startingSector  = startingSector << 8;
    startingSector |= cmd[6];

    sectorCount  = cmd[8];                     // get the # of sectors to read
    sectorCount  = sectorCount << 8;
    sectorCount |= cmd[9];

    readWriteGeneric(readNotWrite, startingSector, sectorCount);
}

//---------------------------------------------
void Scsi::readWriteGeneric(bool readNotWrite, DWORD startingSector, DWORD sectorCount)
{
    DWORD totalByteCount = sectorCount * 512;

    if(totalByteCount > 0xffffff) {                                     // if trying to send more than 16 MB of data at once, fail
        Debug::out(LOG_ERROR, "Scsi::readWriteGeneric() - tried to transfer more than 16 MB of data of once, fail");

        storeSenseAndSendStatus(SCSI_ST_CHECK_CONDITION, SCSI_E_IllegalRequest, SCSI_ASC_NO_ADDITIONAL_SENSE, SCSI_ASCQ_NO_ADDITIONAL_SENSE);
        return;
    }

    // get device capacity, so we can verify if we're not going out of range
    int64_t mediaCapacityInBytes, mediaCapacityInSectors;
    dataMedia->getCapacity(mediaCapacityInBytes, mediaCapacityInSectors);

    if((startingSector + sectorCount - 1) > mediaCapacityInSectors) {   // trying to read out of range? 
        Debug::out(LOG_ERROR, "Scsi::readWriteGeneric() - tried to go out of range");
        storeSenseAndSendStatus(SCSI_ST_CHECK_CONDITION, SCSI_E_IllegalRequest, SCSI_ASC_LBA_OUT_OF_RANGE, SCSI_ASCQ_NO_ADDITIONAL_SENSE);
        return;
    }

    //--------------------------------
    bool res;

    if(sectorCount < 2048) {        // less than 1 MB?
        if(readNotWrite) {          // if read
            res = readSectors_small   (startingSector, sectorCount);
        } else {                    // if write
            res = writeSectors_small  (startingSector, sectorCount);
        }
    } else {                        // more than 1 MB?
        if(readNotWrite) {          // if read
            res = readSectors_big   (startingSector, sectorCount);
        } else {                    // if write
            res = writeSectors_big  (startingSector, sectorCount);
        }
    }

    if(res) {                                            // if everything was OK
        SendOKstatus();
    } else {                                            // if error
        storeSenseAndSendStatus(SCSI_ST_CHECK_CONDITION, SCSI_E_MediumError, SCSI_ASC_NO_ADDITIONAL_SENSE, SCSI_ASCQ_NO_ADDITIONAL_SENSE);
    }
}

//---------------------------------------------
void Scsi::SCSI_Verify(void)
{
    DWORD startSectorNo;
    WORD sectorCount;

    startSectorNo  = cmd[3];                    // get starting sector #
    startSectorNo  = startSectorNo << 8;
    startSectorNo |= cmd[4];
    startSectorNo  = startSectorNo << 8;
    startSectorNo |= cmd[5];
    startSectorNo  = startSectorNo << 8;
    startSectorNo |= cmd[6];

    sectorCount  = cmd[8];                      // get the # of sectors to read
    sectorCount  = sectorCount << 8;
    sectorCount |= cmd[9];

    if((cmd[2] & 0x02) == 0x02) {               // BytChk == 1? : compare with data
        bool res = compareSectors(startSectorNo, sectorCount);   // compare data

        if(!res) {                              // problem when comparing?
            storeSenseAndSendStatus(SCSI_ST_CHECK_CONDITION, SCSI_E_Miscompare, SCSI_ASC_VERIFY_MISCOMPARE, SCSI_ASCQ_NO_ADDITIONAL_SENSE);
        } else {                                // no problem?
            SendOKstatus();
        }
    } else {                                    // BytChk == 0? : no data comparison
        SendOKstatus();
    }
}

//---------------------------------------------
bool Scsi::readSectors_small(DWORD startSectorNo, DWORD sectorCount)
{
    bool res;

    DWORD totalByteCount = sectorCount * 512;

    Debug::out(LOG_DEBUG, "Scsi::readSectors_small() - startSectorNo: 0x%x, sectorCount: 0x%x", startSectorNo, sectorCount);

    res = dataMedia->readSectors(startSectorNo, sectorCount, dataBuffer);

    if(!res) {
        Debug::out(LOG_DEBUG, "Scsi::readSectors_small() - failed for startSectorNo: 0x%x, sectorCountNow: 0x%x", startSectorNo, sectorCount);
        return false;
    }

    dataTrans->addDataBfr(dataBuffer, totalByteCount, true);

    Debug::out(LOG_DEBUG, "Scsi::readSectors_small() - done with success");
    return true;
}

bool Scsi::readSectors_big(DWORD startSectorNo, DWORD sectorCount)
{
    bool res;

    Debug::out(LOG_DEBUG, "Scsi::readSectors() - startSectorNo: 0x%x, sectorCount: 0x%x -- will do %d loops", startSectorNo, sectorCount, (sectorCount / BUFFER_SIZE_SECTORS) + 1);

    sendDataAndStatus_notJustStatus = false;                            // we're handling the _start(), _transferBlock() manually, so later we need to send the status manually
    
    DWORD totalByteCount = sectorCount * 512;
    res = dataTrans->sendData_start(totalByteCount, SCSI_ST_OK, false); // try to start the read data transfer, without status

    if(!res) {
        Debug::out(LOG_ERROR, "Scsi::readSectors() - sendData_start() failed");
        return false;
    }

    // now transfer the data in big chunks of BUFFER_SIZE_SECTORS
    while(sectorCount > 0) {
        // maximum transfer size is BUFFER_SIZE_SECTORS, so transfer less or exactly that in loop
        DWORD sectorCountNow    = (sectorCount < BUFFER_SIZE_SECTORS) ? sectorCount : BUFFER_SIZE_SECTORS;
        DWORD byteCountNow      = sectorCountNow * 512;

        Debug::out(LOG_DEBUG, "Scsi::readSectors() - will read sectorCountNow: 0x%x, sectors to go: 0x%x", sectorCountNow, sectorCount - sectorCountNow);

        // read sector from media
        res = dataMedia->readSectors(startSectorNo, sectorCountNow, dataBuffer);

        if(!res) {
            Debug::out(LOG_DEBUG, "Scsi::readSectors() - dataMedia->readSectors() failed for startSectorNo: 0x%x, sectorCountNow: 0x%x", startSectorNo, sectorCountNow);
            return false;
        }

        // more to next sectors, decreate sector count
        startSectorNo   += sectorCountNow;
        sectorCount     -= sectorCountNow;

        // now transfer this block, which is up to BUFFER_SIZE_SECTORS big
        res = dataTrans->sendData_transferBlock(dataBuffer, byteCountNow);

        if(!res) {
            Debug::out(LOG_DEBUG, "Scsi::readSectors() - dataTrans->sendData_transferBlock() failed for startSectorNo: 0x%x, sectorCountNow: 0x%x", startSectorNo, sectorCountNow);
            return false;
        }
    }

    Debug::out(LOG_DEBUG, "Scsi::readSectors() - done with success");
    return true;
}

//---------------------------------------------
bool Scsi::writeSectors_small(DWORD startSectorNo, DWORD sectorCount)
{
    bool res;

    Debug::out(LOG_DEBUG, "Scsi::writeSectors_small() - startSectorNo: 0x%x, sectorCount: 0x%x", startSectorNo, sectorCount);
    
    DWORD totalByteCount = sectorCount * 512;
    res = dataTrans->recvData(dataBuffer, totalByteCount);

    if(!res) {
        Debug::out(LOG_ERROR, "Scsi::writeSectors() - recvData() failed");
        return false;
    }

    res = dataMedia->writeSectors(startSectorNo, sectorCount, dataBuffer);   // write to media

    if(!res) {
        Debug::out(LOG_ERROR, "Scsi::writeSectors() - dataMedia->writeSectors() failed");
        return false;
    }

    Debug::out(LOG_DEBUG, "Scsi::writeSectors() - done with success");
    return true;
}

bool Scsi::writeSectors_big(DWORD startSectorNo, DWORD sectorCount)
{
    bool res;

    Debug::out(LOG_DEBUG, "Scsi::writeSectors() - startSectorNo: 0x%x, sectorCount: 0x%x -- will do %d loops", startSectorNo, sectorCount, (sectorCount / BUFFER_SIZE_SECTORS) + 1);
    
    sendDataAndStatus_notJustStatus = false;                // we're handling the _start(), _transferBlock() manually, so later we need to send the status manually

    DWORD totalByteCount = sectorCount * 512;
    res = dataTrans->recvData_start(totalByteCount);        // try to start the write data transfer    

    if(!res) {
        Debug::out(LOG_ERROR, "Scsi::writeSectors() - recvData_start() failed");
        return false;
    }

    // now transfer the data in big chunks of BUFFER_SIZE_SECTORS
    while(sectorCount > 0) {
        // maximum transfer size is BUFFER_SIZE_SECTORS, so transfer less or exactly that in loop
        DWORD sectorCountNow    = (sectorCount < BUFFER_SIZE_SECTORS) ? sectorCount : BUFFER_SIZE_SECTORS;
        DWORD byteCountNow      = sectorCountNow * 512;

        Debug::out(LOG_DEBUG, "Scsi::writeSectors() - will write sectorCountNow: 0x%x, sectors to go: 0x%x", sectorCountNow, sectorCount - sectorCountNow);

        // get data from ST
        res = dataTrans->recvData_transferBlock(dataBuffer, byteCountNow);

        if(!res) {
            Debug::out(LOG_ERROR, "Scsi::writeSectors() - dataTrans->recvData_transferBlock() failed");
            return false;
        }

        res = dataMedia->writeSectors(startSectorNo, sectorCountNow, dataBuffer);   // write to media

        if(!res) {
            Debug::out(LOG_ERROR, "Scsi::writeSectors() - dataMedia->writeSectors() failed");
            return false;
        }

        // more to next sectors, decreate sector count
        startSectorNo   += sectorCountNow;
        sectorCount     -= sectorCountNow;
    }

    Debug::out(LOG_DEBUG, "Scsi::writeSectors() - done with success");
    return true;
}

//---------------------------------------------
bool Scsi::compareSectors(DWORD startSectorNo, DWORD sectorCount)
{
    bool res;

    Debug::out(LOG_DEBUG, "Scsi::compareSectors() - startSectorNo: %d, sectorCount: %d -- will do %d loops", startSectorNo, sectorCount, (sectorCount / BUFFER_SIZE_SECTORS) + 1);

    DWORD totalByteCount = sectorCount * 512;
    res = dataTrans->recvData_start(totalByteCount);        // try to start the write data transfer    

    if(!res) {
        Debug::out(LOG_ERROR, "Scsi::compareSectors() - recvData_start() failed");
        return false;
    }

    // now transfer the data in big chunks of BUFFER_SIZE_SECTORS
    while(sectorCount > 0) {
        // maximum transfer size is BUFFER_SIZE_SECTORS, so transfer less or exactly that in loop
        DWORD sectorCountNow    = (sectorCount < BUFFER_SIZE_SECTORS) ? sectorCount : BUFFER_SIZE_SECTORS;
        DWORD byteCountNow      = sectorCountNow * 512;

        // get data from ST
        res = dataTrans->recvData_transferBlock(dataBuffer, byteCountNow);

        if(!res) {
            Debug::out(LOG_ERROR, "Scsi::compareSectors() - dataTrans->recvData_transferBlock() failed");
            return false;
        }

        res = dataMedia->readSectors(startSectorNo, sectorCountNow, dataBuffer2);   // and get data from media

        if(!res) {
            Debug::out(LOG_ERROR, "Scsi::compareSectors() - dataMedia->readSectors() failed");
            return false;
        }

        int iRes = memcmp(dataBuffer, dataBuffer2, byteCountNow);   // now compare the data

        if(iRes != 0) {                                             // data is different?
            Debug::out(LOG_ERROR, "Scsi::compareSectors() - data mismatch");
            return false;
        }

        // more to next sectors, decreate sector count
        startSectorNo   += sectorCountNow;
        sectorCount     -= sectorCountNow;
    }

    return true;
}

