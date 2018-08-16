#ifndef VBL_H
#define VBL_H

#include "global.h"

extern WORD fromVbl;      // if non-zero, then update_con_info() was called from VBL
extern WORD vblInstalled; // if non-zero, the VBL Interrupt handler was installed
extern WORD vblEnabled;   // set to 0 to disable my VBL execution (other VBL routines will run)

void install_vbl(void);   // install vbl handler

#endif
