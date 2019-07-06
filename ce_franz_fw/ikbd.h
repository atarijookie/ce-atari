#ifndef _IKBD_H_
#define _IKBD_H_

#include "defs.h"
#include "main.h"
#include "global_vars.h"

void circularInit(volatile TCircBuffer *cb);
void cicrularAdd(volatile TCircBuffer *cb, BYTE val);
BYTE cicrularGet(volatile TCircBuffer *cb);

#endif
