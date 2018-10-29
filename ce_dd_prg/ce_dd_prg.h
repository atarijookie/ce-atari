//vim : expandtab shiftwidth=4 tabstop=4
#ifndef CE_DD_PRG_H
#define CE_DD_PRG_H

/* defines global variables */

/* include global.h for type definitions (BYTE/WORD/DWORD) */
#include "global.h"

/* include acsi.h for CMD_LENGTH_* definitions */
#include "../ce_hdd_if/hdd_if.h"

extern BYTE commandShort[CMD_LENGTH_SHORT];
extern BYTE commandLong[CMD_LENGTH_LONG];
extern BYTE *pDmaBuffer;

extern BYTE *pDta;
extern BYTE *pDtaBuffer;
extern BYTE fsnextIsForUs;

extern WORD ceDrives;
extern WORD ceMediach;
extern BYTE currentDrive;

#endif /* CE_DD_PRG_H */
