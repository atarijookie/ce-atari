#ifndef __COSMOSOLO_H_
#define __COSMOSOLO_H_

#include "defs.h"

#define TEST_READ           0x90
#define TEST_WRITE          0x91
#define TEST_GET_ACSI_IDS   0x92

#define DEVTYPE_OFF         0
#define DEVTYPE_SD          1
#define DEVTYPE_RAW         2
#define DEVTYPE_TRANSLATED  3

void processCosmoSoloCommands(BYTE isIcd);
BYTE cosmoSoloSetNewId       (void);
BYTE soloTestReadNotWrite    (BYTE readNotWrite);
BYTE soloTestGetACSIids      (void);

#endif
