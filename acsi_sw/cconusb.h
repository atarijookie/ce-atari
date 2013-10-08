#ifndef CCONUSB_H
#define CCONUSB_H

#include <windows.h>
#include <QLibrary>

#include "ftd2xx.h"

typedef
    FT_STATUS WINAPI (*tFT_Open)(
    int deviceNumber,
    FT_HANDLE *pHandle
    );

typedef
    FT_STATUS WINAPI (*tFT_ListDevices)(
    PVOID pArg1,
    PVOID pArg2,
    DWORD Flags
    );

typedef
    FT_STATUS WINAPI (*tFT_Close)(
    FT_HANDLE ftHandle
    );

typedef
    FT_STATUS WINAPI (*tFT_Read)(
    FT_HANDLE ftHandle,
    LPVOID lpBuffer,
    DWORD dwBytesToRead,
    LPDWORD lpBytesReturned
    );

typedef
    FT_STATUS WINAPI (*tFT_Write)(
    FT_HANDLE ftHandle,
    LPVOID lpBuffer,
    DWORD dwBytesToWrite,
    LPDWORD lpBytesWritten
    );

typedef
    FT_STATUS WINAPI (*tFT_GetStatus)(
    FT_HANDLE ftHandle,
    DWORD *dwRxBytes,
    DWORD *dwTxBytes,
    DWORD *dwEventDWord
    );

typedef
    FT_STATUS WINAPI (*tFT_SetBaudRate)(
    FT_HANDLE ftHandle,
    ULONG BaudRate
    );

typedef
    FT_STATUS WINAPI (*tFT_SetDataCharacteristics)(
    FT_HANDLE ftHandle,
    UCHAR WordLength,
    UCHAR StopBits,
    UCHAR Parity
    );

typedef
    FT_STATUS WINAPI (*tFT_SetTimeouts) (
    FT_HANDLE ftHandle,
    ULONG ReadTimeout,
    ULONG WriteTimeout
    );

typedef
    FT_STATUS WINAPI (*tFT_SetLatencyTimer) (
    FT_HANDLE ftHandle,
    UCHAR ucLatency
    );

typedef
    FT_STATUS WINAPI (*tFT_SetUSBParameters) (
    FT_HANDLE ftHandle,
    ULONG ulInTransferSize,
    ULONG ulOutTransferSize
    );

//------------------------------------------------

class CConUsb: public QObject
{
    Q_OBJECT

public:
    CConUsb();
    ~CConUsb();

    virtual bool    init(void);
    virtual void    deinit(void);
    virtual DWORD   bytesToReceive(void);
    virtual DWORD   bytesToSend(void);
    virtual void    txRx(int count, BYTE *sendBuffer, BYTE *receiveBufer);
    virtual void    write(int count, BYTE *buffer);
    virtual void    read (int count, BYTE *buffer);
    virtual bool    isConnected(void);

    void tryToConnect(void);
    bool connectionWorking(void);

private:
    bool loadFTDIdll(void);
    void zeroAllVars(void);

    void logString(QString log);

    QLibrary        *FTDIlib;

    tFT_Open                    pFT_Open;
    tFT_ListDevices             pFT_ListDevices;
    tFT_Close                   pFT_Close;
    tFT_Read                    pFT_Read;
    tFT_Write                   pFT_Write;
    tFT_GetStatus               pFT_GetStatus;
    tFT_SetBaudRate             pFT_SetBaudRate;
    tFT_SetDataCharacteristics  pFT_SetDataCharacteristics;
    tFT_SetTimeouts             pFT_SetTimeouts;
    tFT_SetLatencyTimer         pFT_SetLatencyTimer;
    tFT_SetUSBParameters        pFT_SetUSBParameters;

    bool            isLoaded;
    bool            connected;

    FT_HANDLE       ftHandle;
};

#endif // CCONUSB_H
