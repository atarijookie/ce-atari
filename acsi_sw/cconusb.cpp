#include "cconusb.h"
#include "global.h"

extern "C" void outDebugString(const char *format, ...);

#define SWAP_ENDIAN false

CConUsb::CConUsb()
{
    zeroAllVars();
}

CConUsb::~CConUsb()
{
    deinit();
}

bool CConUsb::init(void)
{
    if(!loadFTDIdll()) {
        return false;
    }

    return true;
}

void CConUsb::deinit(void)
{
    if(FTDIlib != NULL) {
        if(connected) {
            (*pFT_Close)(ftHandle);
            connected = false;
        }

        FTDIlib->unload();
        delete FTDIlib;

        zeroAllVars();
    }
}

void CConUsb::zeroAllVars(void)
{
    isLoaded    = false;
    connected   = false;

    pFT_Open                    = NULL;
    pFT_ListDevices             = NULL;
    pFT_Close                   = NULL;
    pFT_Read                    = NULL;
    pFT_Write                   = NULL;
    pFT_GetStatus               = NULL;
    pFT_SetBaudRate             = NULL;
    pFT_SetDataCharacteristics  = NULL;

    FTDIlib         = NULL;

    prevAtnWord.got = false;
    remainingPacketLength    = -1;
}

void CConUsb::applyNoTxRxLimis(void)
{
    setRemainingTxRxLen(NO_REMAINING_LENGTH, NO_REMAINING_LENGTH);  // set no remaining length
}

void CConUsb::receiveAndApplyTxRxLimits(void)
{
    BYTE inBuff[4], outBuff[4];
    memset(outBuff, 0, 4);

    WORD *pwIn = (WORD *) inBuff;

    // get TX LEN and RX LEN
    txRx(4, outBuff, inBuff);

    WORD txLen = swapWord(pwIn[0]);
    WORD rxLen = swapWord(pwIn[1]);

//    outDebugString("TX/RX limits: TX %d WORDs, RX %d WORDs", txLen, rxLen);

    setRemainingTxRxLen(txLen, rxLen);
}

WORD CConUsb::swapWord(WORD val)
{
    WORD tmp = 0;

    tmp  = val << 8;
    tmp |= val >> 8;

    return tmp;
}

void CConUsb::setRemainingTxRxLen(WORD txLen, WORD rxLen)
{
    if(txLen == NO_REMAINING_LENGTH && rxLen == NO_REMAINING_LENGTH) {    // if setting NO_REMAINING_LENGTH
        if(remainingPacketLength != 0) {
            outDebugString("CConUsb - didn't TX/RX enough data, padding with %d zeros! Fix this!", remainingPacketLength);
            memset(paddingBuffer, 0, PADDINGBUFFER_SIZE);
            txRx(remainingPacketLength, paddingBuffer, paddingBuffer, true);
        }
    } else {                    // if setting real limit
        txLen *= 2;             // convert WORD count to BYTE count
        rxLen *= 2;

        if(txLen >= 8) {        // if we should TX more than 8 bytes, subtract 8 (header length)
            txLen -= 8;
        } else {                // shouldn't TX 8 or more bytes? Then don't TX anymore.
            txLen = 0;
        }

        if(rxLen >= 8) {        // if we should RX more than 8 bytes, subtract 8 (header length)
            rxLen -= 8;
        } else {                // shouldn't RX 8 or more bytes? Then don't RX anymore.
            rxLen = 0;
        }
    }

    // The SPI bus is TX and RX at the same time, so we will TX/RX until both are used up.
    // So use the greater count as the limit.
    if(txLen >= rxLen) {
        remainingPacketLength = txLen;
    } else {
        remainingPacketLength = rxLen;
    }
}

bool CConUsb::loadFTDIdll(void)
{
    if(isLoaded) {
        return true;
    }

    outDebugString("USB: initializing library...");

    FTDIlib = new QLibrary("ftd2xx.dll");

    // load library
    if(!FTDIlib->load()) {
        return false;
    }

    // find all functions
    pFT_Open                    = (tFT_Open)                    FTDIlib->resolve("FT_Open");
    pFT_ListDevices             = (tFT_ListDevices)             FTDIlib->resolve("FT_ListDevices");
    pFT_Close                   = (tFT_Close)                   FTDIlib->resolve("FT_Close");
    pFT_Read                    = (tFT_Read)                    FTDIlib->resolve("FT_Read");
    pFT_Write                   = (tFT_Write)                   FTDIlib->resolve("FT_Write");
    pFT_GetStatus               = (tFT_GetStatus)               FTDIlib->resolve("FT_GetStatus");
    pFT_SetBaudRate             = (tFT_SetBaudRate)             FTDIlib->resolve("FT_SetBaudRate");
    pFT_SetDataCharacteristics  = (tFT_SetDataCharacteristics)  FTDIlib->resolve("FT_SetDataCharacteristics");
    pFT_SetTimeouts             = (tFT_SetTimeouts)             FTDIlib->resolve("FT_SetTimeouts");
    pFT_SetLatencyTimer         = (tFT_SetLatencyTimer)         FTDIlib->resolve("FT_SetLatencyTimer");
    pFT_SetUSBParameters        = (tFT_SetUSBParameters)        FTDIlib->resolve("FT_SetUSBParameters");

    // if functions not found, fail
    if(!pFT_Open || !pFT_ListDevices || !pFT_Close || !pFT_Read || !pFT_Write
       || !pFT_GetStatus || !pFT_SetBaudRate || !pFT_SetTimeouts || !pFT_SetLatencyTimer || !pFT_SetUSBParameters) {
        FTDIlib->unload();
        delete FTDIlib;

        zeroAllVars();

        return false;
    }

    outDebugString("USB: library initialization done.");

    // done
    isLoaded = true;
    return true;
}

void CConUsb::tryToConnect(void)
{
    if(!isLoaded) {                     // if not loaded, don't do anything
        return;
    }

    if(connected) {                     // if already connected, don't have to do anything
        return;
    }

    // Get the number of devices currently connected - in numDevs
    FT_STATUS   ftStatus;
    DWORD       numDevs;

    ftStatus = (*pFT_ListDevices)(&numDevs, NULL, FT_LIST_NUMBER_ONLY);

    if (ftStatus != FT_OK) {                // failed to get # of devices?
        outDebugString("USB: failed to get number of connected devices.");
        return;
    }

    // if there are no devices connected
    if(numDevs < 1) {
        return;
    }

    for(DWORD i=0; i<numDevs; i++) {
       // try to open device #0
       ftStatus = (*pFT_Open)(i, &ftHandle);

       if (ftStatus != FT_OK) {
           outDebugString("USB: failed to open device...");
           continue;
       }

       connected = true;
       outDebugString("USB: connected to device.");

       // if it's FT232, set baud rate
//       ftStatus = (*pFT_SetBaudRate)(ftHandle, 115200);
//       if (ftStatus != FT_OK) {
//           outDebugString("USB: speed not set");
//       }

//       ftStatus = (*pFT_SetDataCharacteristics) (ftHandle, FT_BITS_8, FT_STOP_BITS_1, FT_PARITY_NONE);
//       if (ftStatus != FT_OK) {
//           outDebugString("USB: data characteristics not set");
//       }

       (*pFT_SetTimeouts)(ftHandle, 10, 10);
       (*pFT_SetLatencyTimer)(ftHandle, 5);
//       (*pFT_SetUSBParameters)(ftHandle, 64, 256);

       break;
    }
}

bool CConUsb::connectionWorking(void)
{
    FT_STATUS   ftStatus;
    DWORD dwRxBytes, dwTxBytes, dwEventDWord;
    ftStatus = (*pFT_GetStatus)(ftHandle, &dwRxBytes, &dwTxBytes, &dwEventDWord);

    if(ftStatus == FT_OK) {
        return true;
    }

    return false;
}

DWORD CConUsb::bytesToReceive(void)
{
    if(!connected) {
        return 0;
    }

    DWORD dwRxBytes, dwTxBytes, dwEventDWord;
    (*pFT_GetStatus)(ftHandle, &dwRxBytes, &dwTxBytes, &dwEventDWord);

    return dwRxBytes;
}

DWORD CConUsb::bytesToSend(void)
{
    if(!connected) {
        return 0;
    }

    DWORD dwRxBytes, dwTxBytes, dwEventDWord;
    (*pFT_GetStatus)(ftHandle, &dwRxBytes, &dwTxBytes, &dwEventDWord);

    return dwTxBytes;
}

WORD CConUsb::getRemainingLength(void)
{
    return remainingPacketLength;
}

void CConUsb::txRx(int count, BYTE *sendBuffer, BYTE *receiveBufer, bool addLastToAtn)
{
    if(SWAP_ENDIAN) {       // swap endian on sending if required
        BYTE tmp;

        for(int i=0; i<count; i += 2) {
            tmp             = sendBuffer[i+1];
            sendBuffer[i+1] = sendBuffer[i];
            sendBuffer[i]   = tmp;
        }
    }

    if(count == TXRX_COUNT_REST) {          // if should TX/RX the rest, use the remaining length
        count = remainingPacketLength;
    }

    if(remainingPacketLength != NO_REMAINING_LENGTH) {
        if(count > remainingPacketLength) {
            outDebugString("CConUsb::txRx - trying to TX/RX %d more bytes then allowed! Fix this!", (count - remainingPacketLength));

            count = remainingPacketLength;
        }
    }

    write   (count, sendBuffer);
    read    (count, receiveBufer);

    if(remainingPacketLength != NO_REMAINING_LENGTH) {
        remainingPacketLength -= count;             // mark that we've send this much data
    }

    // add the last WORD as possible to check for the new ATN
    if(addLastToAtn) {
        setAtnWord(&receiveBufer[count - 2]);
    }
}

void CConUsb::write(int count, BYTE *buffer)
{
    DWORD bytesWrote;
    int remaining = count;
    int wroteTotal = 0;

    DWORD start = GetTickCount();

    while(remaining > 0) {
        if((GetTickCount() - start) > 3000) {         // timeout?
            outDebugString("Timeout on USB write!");
            outDebugString("remaining: %d", remaining);
            return;
        }

        (*pFT_Write)(ftHandle, &buffer[wroteTotal], remaining, &bytesWrote);

        remaining  -= bytesWrote;
        wroteTotal += bytesWrote;
    }
}

void CConUsb::read (int count, BYTE *buffer)
{
    DWORD bytesRead;
    int remaining = count;
    int readTotal = 0;

    DWORD start = GetTickCount();

    while(remaining > 0) {
        if((GetTickCount() - start) > 3000) {         // timeout?
            outDebugString("Timeout on USB read!");
            return;
        }

        (*pFT_Read)(ftHandle, &buffer[readTotal], remaining, &bytesRead);

        remaining -= bytesRead;
        readTotal += bytesRead;
    }
}

bool CConUsb::isConnected(void)
{
    return connected;
}

void CConUsb::getAtnWord(BYTE *bfr)
{
    if(prevAtnWord.got) {                   // got some previous ATN word? use it
        bfr[0] = prevAtnWord.bytes[0];
        bfr[1] = prevAtnWord.bytes[1];
        prevAtnWord.got = false;

        return;
    }

    // no previous ATN word? read it!
    BYTE outBuff[2];
    memset(outBuff, 0, 2);
    txRx(2, outBuff, bfr, false);
}

void CConUsb::setAtnWord(BYTE *bfr)
{
    prevAtnWord.bytes[0] = bfr[0];
    prevAtnWord.bytes[1] = bfr[1];
    prevAtnWord.got = true;
}


