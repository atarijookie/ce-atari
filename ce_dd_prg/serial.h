#ifndef __SERIAL_H_
#define __SERIAL_H_

#include "global.h"

//#define SERIALDEBUG     TRUE
#define SERIALDEBUG     FALSE

void aux_sendChar  (char    c);
void aux_sendString(char *str);
void aux_hexNibble (uint8_t  val);
void aux_hexByte   (uint8_t  val);
void aux_hexWord   (uint16_t  val);
void aux_hexDword  (uint32_t val);

#endif
