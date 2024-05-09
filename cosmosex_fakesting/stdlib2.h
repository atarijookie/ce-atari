// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
#ifndef _STDLIB2_H_
#define _STDLIB2_H_

#include <stdint.h>
#include "globdefs.h"

int     strcmp ( const char * str1, const char * str2);

uint32_t getTicks_fromSupervisor(void);
uint32_t getTicks(void);

uint8_t *storeByte(uint8_t *bfr, uint8_t value);
uint8_t *storeWord(uint8_t *bfr, uint16_t value);
uint8_t *storeDword(uint8_t *bfr, uint32_t value);

uint16_t  getWord(uint8_t *bfr);
uint32_t getDword(uint8_t *bfr);

uint16_t   getWordByByteOffset (void *base, int ofs);
uint32_t  getDwordByByteOffset(void *base, int ofs);
void  *getVoidpByByteOffset(void *base, int ofs);
void   setWordByByteOffset (void *base, int ofs, uint16_t val);
void   setDwordByByteOffset(void *base, int ofs, uint32_t val);

    #ifdef DEBUG_STRING
    void logStr(char *str);
    void logBfr(uint8_t *bfr, int len);
    #endif

#endif
