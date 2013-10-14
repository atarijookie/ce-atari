//------------------------------------------------------
#include <stdio.h>
#include "mydefines.h"
//------------------------------------------------------
BYTE rtc_GetClock(BYTE *buffer);
BYTE rtc_SetClock(WORD year, BYTE mon, BYTE day, BYTE hou, BYTE min, BYTE sec);

void rtc_write(BYTE address, BYTE data, BYTE WriteData);
BYTE rtc_read(BYTE address);

BYTE rtc_writeByte(BYTE data);
BYTE rtc_readByte(BYTE isLast);

BYTE Dec2Bin(BYTE dec);
BYTE Bin2Dec(BYTE dec);

void I2C_L(WORD what);
void I2C_H(WORD what);
//------------------------------------------------------
