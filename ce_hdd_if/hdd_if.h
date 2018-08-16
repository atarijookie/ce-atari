#ifndef _HDD_IF_H_
#define _HDD_IF_H_

#include "global.h"

// These is higher level API which should be used in apps using this lib.

typedef void  (*THddIfCmd) (BYTE readNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount);
typedef void  (*TsetReg)   (int whichReg, DWORD value);
typedef DWORD (*TgetReg)   (int whichReg);

typedef BYTE  (*TdmaDataTx_prepare) (BYTE readNotWrite, BYTE *buffer, DWORD dataByteCount);
typedef BYTE  (*TdmaDataTx_do)      (BYTE readNotWrite, BYTE *buffer, DWORD dataByteCount);

//--------------------------------
typedef volatile BYTE mutex;

// return 1 if lock could be aquired, 0 if it was already locked
inline BYTE mutex_trylock(mutex *mtx);
inline void mutex_unlock(mutex *mtx);
//--------------------------------

typedef struct {
    THddIfCmd           cmd;
    THddIfCmd           cmd_nolock;
    THddIfCmd           cmd_intern;

    BYTE                success;
    BYTE                statusByte;
    BYTE                phaseChanged;

    int                 retriesDoneCount;
    int                 maxRetriesCount;

    BYTE                forceFlock;

    TsetReg             pSetReg;
    TgetReg             pGetReg;

    TdmaDataTx_prepare  pDmaDataTx_prepare;
    TdmaDataTx_do       pDmaDataTx_do;

    BYTE                scsiHostId;
    volatile mutex      mtx;            // used for avoiding colision between main and interrupt progs accessing disk
} THDif;

extern THDif hdIf;

//--------------------------------
#define IF_NONE         0
#define IF_ACSI         1
#define IF_SCSI_TT      2
#define IF_SCSI_FALCON  3

void hdd_if_select(int ifType); // call this function with above define as a param to init the pointers depending on that interface

#define DEVICE_NOT_FOUND    0xff
BYTE findDevice(void);          // returns device ID (0-7) or DEVICE_NOT_FOUND (0xff)

//--------------------------------

#define OK          0           // OK status
#define ACSIERROR   0xff        // ERROR status (timeout)

#define MAXSECTORS          254 // Max # sectors for a DMA

#define ACSI_READ           1
#define ACSI_WRITE          0

#define CMD_LENGTH_SHORT    6
#define CMD_LENGTH_LONG     13

//--------------------------------

#endif
