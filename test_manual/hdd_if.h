#ifndef _HDD_IF_H_
#define _HDD_IF_H_

#include "global.h"

BYTE scsi_cmd_TT           (BYTE readNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount);
BYTE scsi_cmd_Falcon       (BYTE readNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount);

typedef BYTE  (*THddIfCmd) (BYTE readNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount);
typedef void  (*TsetReg)   (int whichReg, DWORD value);
typedef DWORD (*TgetReg)   (int whichReg);

typedef BYTE  (*TdmaDataTx_prepare) (BYTE readNotWrite, BYTE *buffer, DWORD dataByteCount);
typedef BYTE  (*TdmaDataTx_do)      (BYTE readNotWrite, BYTE *buffer, DWORD dataByteCount);

DWORD scsi_getReg_TT(int whichReg);
void  scsi_setReg_TT(int whichReg, DWORD value);

DWORD scsi_getReg_Falcon(int whichReg);
void  scsi_setReg_Falcon(int whichReg, DWORD value);

void  scsi_clrBit(int whichReg, DWORD bitMask);
void  scsi_setBit(int whichReg, DWORD bitMask);

BYTE dmaDataTx_prepare_TT       (BYTE readNotWrite, BYTE *buffer, DWORD dataByteCount);
BYTE dmaDataTx_do_TT            (BYTE readNotWrite, BYTE *buffer, DWORD dataByteCount);

BYTE dmaDataTx_prepare_Falcon   (BYTE readNotWrite, BYTE *buffer, DWORD dataByteCount);
BYTE dmaDataTx_do_Falcon        (BYTE readNotWrite, BYTE *buffer, DWORD dataByteCount);

typedef struct {
    THddIfCmd           hddIfCmd;

    TsetReg             pSetReg;
    TgetReg             pGetReg;

    TdmaDataTx_prepare  pDmaDataTx_prepare;
    TdmaDataTx_do       pDmaDataTx_do;
    
    BYTE                scsiHostId;
} THDif;

extern THDif hdIf;

//--------------------------------
#define IF_NONE         0
#define IF_ACSI         1
#define IF_SCSI_TT      2
#define IF_SCSI_FALCON  3

void hdd_if_select(int ifType);     // call this function with above define as a param to init the pointers depending on that interface
//--------------------------------

#endif
