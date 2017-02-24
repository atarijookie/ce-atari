#ifndef __SERIAL_H_
#define __SERIAL_H_

#include "global.h"

//#define SERIALDEBUG     TRUE
#define SERIALDEBUG     FALSE

void aux_sendChar  (char    c);
void aux_sendString(char *str);
void aux_hexNibble (BYTE  val);
void aux_hexByte   (BYTE  val);
void aux_hexWord   (WORD  val);
void aux_hexDword  (DWORD val);

#endif
