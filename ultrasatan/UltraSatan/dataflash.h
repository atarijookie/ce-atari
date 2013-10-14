//-----------------------------------------------
#include <stdio.h>
#include "mydefines.h"
//-----------------------------------------------
#define DATAFLASH_ID	123
#define	ALL_CS_PINS		0xff

// public dataflash functions for normal usage
BYTE Flash_ReadStatusByte(BYTE *value);
BYTE Flash_GetIsReady(void);
BYTE Flash_GetIsProtected(void);

BYTE Flash_ReadPage(DWORD page, BYTE *buffer);
BYTE Flash_WritePage(DWORD page, BYTE *buffer);

// private dataflash functions
void ShortDelay(void);
void Flash_WriteBuffer(BYTE *buffer);
//-----------------------------------------------


