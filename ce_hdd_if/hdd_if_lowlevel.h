#ifndef _HDD_IF_LL_H_
#define _HDD_IF_LL_H_

// These are low level functions, which should be called only inside the library, not directly in app

#include "global.h"

void scsi_cmd_TT           (BYTE readNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount);
void scsi_cmd_Falcon       (BYTE readNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount);

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

#endif
