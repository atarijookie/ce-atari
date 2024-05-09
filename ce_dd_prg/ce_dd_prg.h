//vim : expandtab shiftwidth=4 tabstop=4
#ifndef CE_DD_PRG_H
#define CE_DD_PRG_H

/* defines global variables */

/* include global.h for type definitions (uint8_t/uint16_t/uint32_t) */
#include "global.h"
/* include acsi.h for CMD_LENGTH_* definitions */
#include "../libacsiscsi/acsi.h"

extern uint8_t commandShort[CMD_LENGTH_SHORT];
extern uint8_t commandLong[CMD_LENGTH_LONG];
extern uint8_t *pDmaBuffer;

extern uint8_t deviceID;

extern uint8_t *pDta;
extern uint8_t *pDtaBuffer;
extern uint8_t fsnextIsForUs;

extern uint16_t ceDrives;
extern uint16_t ceMediach;
extern uint8_t currentDrive;

#endif /* CE_DD_PRG_H */
