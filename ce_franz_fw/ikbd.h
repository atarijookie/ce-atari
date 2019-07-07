#ifndef _IKBD_H_
#define _IKBD_H_

#include "defs.h"
#include "main.h"
#include "global_vars.h"

void circularInit(TCircBuffer *cb);
void cicrularAdd(TCircBuffer *cb, BYTE val);
BYTE cicrularGet(TCircBuffer *cb);

#endif
