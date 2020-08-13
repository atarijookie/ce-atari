#include <string.h>

#include "chipinterface12.h"
#include "gpio.h"
#include "../utils.h"
#include "../debug.h"

ChipInterface12::ChipInterface12()
{
    conSpi = new CConSpi();

    bufOut = new BYTE[MFM_STREAM_SIZE];
    bufIn = new BYTE[MFM_STREAM_SIZE];

    memset(&hansConfigWords, 0, sizeof(hansConfigWords));
}

ChipInterface12::~ChipInterface12()
{
    delete conSpi;

    delete []bufOut;
    delete []bufIn;
}

bool ChipInterface12::open(void)
{
    return gpio_open();
}

void ChipInterface12::close(void)
{
    gpio_close();
}

void ChipInterface12::ikdbUartEnable(bool enable)
{
    if(enable) {
        bcm2835_gpio_write(PIN_TX_SEL1N2, HIGH);            // TX_SEL1N2, switch the RX line to receive from Franz, which does the 9600 to 7812 baud translation
    }
}

void ChipInterface12::resetHDDandFDD(void)
{
    bcm2835_gpio_write(PIN_RESET_HANS,          LOW);       // reset lines to RESET state
    bcm2835_gpio_write(PIN_RESET_FRANZ,         LOW);

    Utils::sleepMs(10);                                     // wait a while to let the reset work

    bcm2835_gpio_write(PIN_RESET_HANS,          HIGH);      // reset lines to RUN (not reset) state
    bcm2835_gpio_write(PIN_RESET_FRANZ,         HIGH);

    Utils::sleepMs(50);                                     // wait a while to let the devices boot
}

void ChipInterface12::resetHDD(void)
{
    bcm2835_gpio_write(PIN_RESET_HANS,          LOW);       // reset lines to RESET state
    Utils::sleepMs(10);                                     // wait a while to let the reset work
    bcm2835_gpio_write(PIN_RESET_HANS,          HIGH);      // reset lines to RUN (not reset) state
    Utils::sleepMs(50);                                     // wait a while to let the devices boot
}

void ChipInterface12::resetFDD(void)
{
    bcm2835_gpio_write(PIN_RESET_FRANZ,         LOW);
    Utils::sleepMs(10);                                     // wait a while to let the reset work
    bcm2835_gpio_write(PIN_RESET_FRANZ,         HIGH);
    Utils::sleepMs(50);                                     // wait a while to let the devices boot
}

bool ChipInterface12::actionNeeded(bool &hardNotFloppy, BYTE *inBuf)
{
    // if waitForATN() succeeds, it fills 8 bytes of data in buffer
    // ...but then we might need some little more, so let's determine what it was
    // and keep reading as much as needed
    int moreData = 0;

    // check for any ATN code waiting from Hans
    bool res = conSpi->waitForATN(SPI_CS_HANS, (BYTE) ATN_ANY, 0, inBuf);

    if(res) {    // HANS is signaling attention?
        if(inBuf[3] == ATN_ACSI_COMMAND) {
            moreData = 14;                              // all ACSI command bytes
        }

        if(moreData) {
            conSpi->txRx(SPI_CS_HANS, moreData, bufOut, inBuf + 8);   // get more data, offset in input buffer by header size
        }

        hardNotFloppy = true;
        return true;
    }

    // check for any ATN code waiting from Franz
    res = conSpi->waitForATN(SPI_CS_FRANZ, (BYTE) ATN_ANY, 0, inBuf);

    if(res) {    // FRANZ is signaling attention?
        if(inBuf[3] == ATN_SEND_TRACK) {
            moreData = 2;                               // side + track
        }

        if(moreData) {
            conSpi->txRx(SPI_CS_FRANZ, moreData, bufOut, inBuf + 8);   // get more data, offset in input buffer by header size
        }

        hardNotFloppy = false;
        return true;
    }

    // no action needed
    return false;
}

#define MAKEWORD(A, B)  ( (((WORD)A)<<8) | ((WORD)B) )

#define HDD_FW_RESPONSE_LEN     12

void ChipInterface12::setHDDconfig(BYTE hddEnabledIDs, BYTE sdCardId, BYTE fddEnabledSlots, bool setNewFloppyImageLed, BYTE newFloppyImageLed)
{
    memset(bufOut, 0, HDD_FW_RESPONSE_LEN);

    // WORD sent (bytes shown): 01 23 45 67

    responseStart(HDD_FW_RESPONSE_LEN);                         // init the response struct

    hansConfigWords.next.acsi   = MAKEWORD(hddEnabledIDs, sdCardId);
    hansConfigWords.next.fdd    = MAKEWORD(fddEnabledSlots, 0);

    if( (hansConfigWords.next.acsi  != hansConfigWords.current.acsi) ||
        (hansConfigWords.next.fdd   != hansConfigWords.current.fdd )) {

        // hansConfigWords.skipNextSet - it's a flag used for skipping one config sending, because we send the new config now, but receive it processed in the next (not this) fw version packet

        if(!hansConfigWords.skipNextSet) {
            responseAddWord(bufOut, CMD_ACSI_CONFIG);             // CMD: send acsi config
            responseAddWord(bufOut, hansConfigWords.next.acsi);   // store ACSI enabled IDs and which ACSI ID is used for SD card
            responseAddWord(bufOut, hansConfigWords.next.fdd);    // store which floppy images are enabled

            hansConfigWords.skipNextSet = true;                 // we have just sent the config, skip the next sending, so we won't send it twice in a row
        } else {                                                // if we should skip sending config this time, then don't skip it next time (if needed)
            hansConfigWords.skipNextSet = false;
        }
    }
    
    //--------------
    if(setNewFloppyImageLed) {
        responseAddWord(bufOut, CMD_FLOPPY_SWITCH);               // CMD: set new image LED (bytes 8 & 9)
        responseAddWord(bufOut, MAKEWORD(fddEnabledSlots, newFloppyImageLed));  // store which floppy images LED should be on
    }
}

#define FDD_FW_RESPONSE_LEN     14

void ChipInterface12::setFDDconfig(bool setFloppyConfig, bool fddEnabled, int id, int writeProtected, bool setDiskChanged, bool diskChanged)
{
    memset(bufOut, 0, FDD_FW_RESPONSE_LEN);
    
    responseStart(FDD_FW_RESPONSE_LEN);                             // init the response struct

    if(setFloppyConfig) {                                   // should set floppy config?
        responseAddByte(bufOut, ( fddEnabled     ? CMD_DRIVE_ENABLED     : CMD_DRIVE_DISABLED) );
        responseAddByte(bufOut, ((id == 0)       ? CMD_SET_DRIVE_ID_0    : CMD_SET_DRIVE_ID_1) );
        responseAddByte(bufOut, ( writeProtected ? CMD_WRITE_PROTECT_ON  : CMD_WRITE_PROTECT_OFF) );
    }

    if(setDiskChanged) {
        responseAddByte(bufOut, ( diskChanged    ? CMD_DISK_CHANGE_ON    : CMD_DISK_CHANGE_OFF) );
    }
}

void ChipInterface12::getFWversion(bool hardNotFloppy, BYTE *inFwVer)
{
    if(hardNotFloppy) {     // for HDD
        // bufOut should be filled with Hans config - by calling setHDDconfig() (and not calling anything else inbetween)
        conSpi->txRx(SPI_CS_HANS, HDD_FW_RESPONSE_LEN, bufOut, inFwVer);

        hansConfigWords.current.acsi = MAKEWORD(inFwVer[6], inFwVer[7]);
        hansConfigWords.current.fdd  = MAKEWORD(inFwVer[8],        0);
    } else {                // for FDD
        // bufOut should be filled with Franz config - by calling setFDDconfig() (and not calling anything else inbetween)
        conSpi->txRx(SPI_CS_FRANZ, FDD_FW_RESPONSE_LEN, bufOut, inFwVer);
    }
}

void ChipInterface12::responseStart(int bufferLengthInBytes)        // use this to start creating response (commands) to Hans or Franz
{
    response.bfrLengthInBytes   = bufferLengthInBytes;
    response.currentLength      = 0;
}

void ChipInterface12::responseAddWord(BYTE *bfr, WORD value)        // add a WORD to the response (command) to Hans or Franz
{
    if(response.currentLength >= response.bfrLengthInBytes) {
        return;
    }

    bfr[response.currentLength + 0] = (BYTE) (value >> 8);
    bfr[response.currentLength + 1] = (BYTE) (value & 0xff);
    response.currentLength += 2;
}

void ChipInterface12::responseAddByte(BYTE *bfr, BYTE value)        // add a BYTE to the response (command) to Hans or Franz
{
    if(response.currentLength >= response.bfrLengthInBytes) {
        return;
    }

    bfr[response.currentLength] = value;
    response.currentLength++;
}

bool ChipInterface12::hdd_sendData_start(DWORD totalDataCount, BYTE scsiStatus, bool withStatus)
{
    if(totalDataCount > 0xffffff) {
        Debug::out(LOG_ERROR, "AcsiDataTrans::sendData_start -- trying to send more than 16 MB, fail");
        return false;
    }

    memset(bufOut, 0, COMMAND_SIZE);

    bufOut[3] = withStatus ? CMD_DATA_READ_WITH_STATUS : CMD_DATA_READ_WITHOUT_STATUS;  // store command - with or without status
    bufOut[4] = totalDataCount >> 16;                           // store data size
    bufOut[5] = totalDataCount >>  8;
    bufOut[6] = totalDataCount  & 0xff;
    bufOut[7] = scsiStatus;                                     // store status

    conSpi->txRx(SPI_CS_HANS, COMMAND_SIZE, bufOut, bufIn);        // transmit this command

    return true;
}

bool ChipInterface12::hdd_sendData_transferBlock(BYTE *pData, DWORD dataCount)
{
    bufOut[0] = 0;
    bufOut[1] = CMD_DATA_MARKER;                                  // mark the start of data

    if((dataCount & 1) != 0) {                                      // odd number of bytes? make it even, we're sending words...
        dataCount++;
    }
    
    while(dataCount > 0) {                                          // while there's something to send
        bool res = conSpi->waitForATN(SPI_CS_HANS, ATN_READ_MORE_DATA, 1000, bufIn);   // wait for ATN_READ_MORE_DATA

        if(!res) {                                                  // this didn't come? fuck!
            return false;
        }

        DWORD cntNow = (dataCount > 512) ? 512 : dataCount;         // max 512 bytes per transfer

        memcpy(bufOut + 2, pData, cntNow);                          // copy the data after the header (2 bytes)
        conSpi->txRx(SPI_CS_HANS, cntNow + 4, bufOut, bufIn);          // transmit this buffer with header + terminating zero (WORD)

        pData       += cntNow;                                      // move the data pointer further
        dataCount   -= cntNow;
    }

    return true;
}

bool ChipInterface12::hdd_recvData_start(BYTE *recvBuffer, DWORD totalDataCount)
{
    if(totalDataCount > 0xffffff) {
        Debug::out(LOG_ERROR, "AcsiDataTrans::recvData_start() -- trying to send more than 16 MB, fail");
        return false;
    }

    memset(bufOut, 0, COMMAND_SIZE);

    // first send the command and tell Hans that we need WRITE data
    bufOut[3] = CMD_DATA_WRITE;                                 // store command - WRITE
    bufOut[4] = totalDataCount >> 16;                           // store data size
    bufOut[5] = totalDataCount >>  8;
    bufOut[6] = totalDataCount  & 0xff;
    bufOut[7] = 0xff;                                           // store INVALID status, because the real status will be sent on CMD_SEND_STATUS

    conSpi->txRx(SPI_CS_HANS, COMMAND_SIZE, bufOut, bufIn);        // transmit this command
    return true;
}

bool ChipInterface12::hdd_recvData_transferBlock(BYTE *pData, DWORD dataCount)
{
    memset(bufOut, 0, TX_RX_BUFF_SIZE);                   // nothing to transmit, really...
    BYTE inBuf[8];

    while(dataCount > 0) {
        // request maximum 512 bytes from host
        DWORD subCount = (dataCount > 512) ? 512 : dataCount;

        bool res = conSpi->waitForATN(SPI_CS_HANS, ATN_WRITE_MORE_DATA, 1000, inBuf); // wait for ATN_WRITE_MORE_DATA

        if(!res) {                                          // this didn't come? fuck!
            return false;
        }

        conSpi->txRx(SPI_CS_HANS, subCount + 8 - 4, bufOut, bufIn);    // transmit data (size = subCount) + header and footer (size = 8) - already received 4 bytes
        memcpy(pData, bufIn + 2, subCount);                 // copy just the data, skip sequence number

        dataCount   -= subCount;                            // decreate the data counter
        pData       += subCount;                            // move in the buffer further
    }

    return true;
}

bool ChipInterface12::hdd_sendStatusToHans(BYTE statusByte)
{
#if defined(ONPC_HIGHLEVEL)        
//    Debug::out(LOG_ERROR, "AcsiDataTrans::sendStatusToHans -- sending statusByte %02x", statusByte);

    serverSocket_write(&statusByte, 1);
#elif defined(ONPC_NOTHING) 
    // nothing here
#else
    bool res = conSpi->waitForATN(SPI_CS_HANS, ATN_GET_STATUS, 1000, bufIn);   // wait for ATN_GET_STATUS

    if(!res) {
        return false;
    }

    memset(bufOut, 0, 16);                                // clear the tx buffer
    bufOut[1] = CMD_SEND_STATUS;                          // set the command and the statusByte
    bufOut[2] = statusByte;

    conSpi->txRx(SPI_CS_HANS, 16 - 8, bufOut, bufIn);        // transmit the statusByte (16 bytes total, but 8 already received)
#endif

    return true;
}

void ChipInterface12::fdd_sendTrackToChip(int byteCount, BYTE *encodedTrack)
{
    // send encoded track out, read garbage into bufIn and don't care about it
    conSpi->txRx(SPI_CS_FRANZ, byteCount, encodedTrack, bufIn);
}

BYTE* ChipInterface12::fdd_sectorWritten(int &side, int &track, int &sector, int &byteCount)
{
    byteCount = conSpi->getRemainingLength();               // get how many data we still have

    memset(bufOut, 0, byteCount);                           // clear the output buffer before sending it to Franz (just in case)
    conSpi->txRx(SPI_CS_FRANZ, byteCount, bufOut, bufIn);   // get all the remaining data

    // get the written sector, side, track number
    sector  = bufIn[1];
    track   = bufIn[0] & 0x7f;
    side    = (bufIn[0] & 0x80) ? 1 : 0;

    return bufIn;                                           // return pointer to received written sector
}
