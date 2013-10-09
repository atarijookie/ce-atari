#include "cconusb.h"
#include "global.h"

extern "C" void outDebugString(const char *format, ...);

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

void CConUsb::txRx(int count, BYTE *sendBuffer, BYTE *receiveBufer)
{
    write   (count, sendBuffer);
    read    (count, receiveBufer);
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

