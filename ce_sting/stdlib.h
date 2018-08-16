// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
#ifndef _STDLIB_H_
#define _STDLIB_H_

#include "globdefs.h"

DWORD getTicks_fromSupervisor(void);
DWORD custom_getTicks(void);

BYTE *storeByte(BYTE *bfr, BYTE value);
BYTE *storeWord(BYTE *bfr, WORD value);
BYTE *storeDword(BYTE *bfr, DWORD value);

WORD  getWord(BYTE *bfr);
DWORD getDword(BYTE *bfr);

void showHexByte(int val);
void showHexWord(WORD val);
void showHexDword(DWORD val);

WORD   getWordByByteOffset (void *base, int ofs);
DWORD  getDwordByByteOffset(void *base, int ofs);
void  *getVoidpByByteOffset(void *base, int ofs);
void   setWordByByteOffset (void *base, int ofs, WORD val);
void   setDwordByByteOffset(void *base, int ofs, DWORD val);

    #ifdef DEBUG_STRING
    void logStr(char *str);
    void logBfr(BYTE *bfr, int len);
    #endif

#endif
