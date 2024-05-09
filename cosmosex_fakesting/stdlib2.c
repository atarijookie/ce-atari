// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
#include <mint/sysbind.h>
#include <mint/osbind.h>

#include <stdint.h>

#include "vbl.h"
#include "stdlib.h"

uint8_t showHex_toLogNotScreen;

uint8_t *storeByte(uint8_t *bfr, uint8_t value)
{
    *bfr = (uint8_t) value;                // store byte
    bfr++;                              // advance to next byte

    return bfr;                         // return the updated buffer address
}

uint8_t *storeWord(uint8_t *bfr, uint16_t value)
{
    *bfr = (uint8_t) (value >> 8);         // store higher part
    bfr++;                              // advance to next byte

    *bfr = (uint8_t) (value);              // store lower part
    bfr++;                              // advance to next byte

    return bfr;                         // return the updated buffer address
}

uint8_t *storeDword(uint8_t *bfr, uint32_t value)
{
    *bfr = (uint8_t) (value >> 24);        // store highest part
    bfr++;                              // advance to next byte

    *bfr = (uint8_t) (value >> 16);        // store mid hi part
    bfr++;                              // advance to next byte

    *bfr = (uint8_t) (value >>  8);        // store mid low part
    bfr++;                              // advance to next byte

    *bfr = (uint8_t) (value);              // store lowest part
    bfr++;                              // advance to next byte

    return bfr;                         // return the updated buffer address
}

uint16_t getWord(uint8_t *bfr)
{
    uint16_t val;

    val = ((uint16_t) bfr[0]) << 8;         // get upper part
    val = val | ((uint16_t) bfr[1]);        // get lower part

    return val;
}

uint32_t getDword(uint8_t *bfr)
{
    uint32_t val;

    val =       ((uint32_t) bfr[0]) << 24;
    val = val | ((uint32_t) bfr[1]) << 16;
    val = val | ((uint32_t) bfr[2]) <<  8;
    val = val | ((uint32_t) bfr[3]);

    return val;
}

uint16_t getWordByByteOffset(void *base, int ofs)
{
    uint8_t *pByte     = (uint8_t *) base;
    uint16_t *pWord     = (uint16_t *) (pByte + ofs);
    uint16_t val        = *pWord;
    return val;
}

uint32_t getDwordByByteOffset(void *base, int ofs)
{
    uint8_t  *pByte    = (uint8_t  *)  base;
    uint32_t *pDword   = (uint32_t *) (pByte + ofs);
    uint32_t val       = *pDword;
    return val;
}

void *getVoidpByByteOffset(void *base, int ofs)
{
    void *p = (void *) getDwordByByteOffset(base, ofs);
    return p;
}

void setWordByByteOffset(void *base, int ofs, uint16_t val)
{
    uint8_t *pByte  = (uint8_t *)  base;
    uint16_t *pWord     = (uint16_t *) (pByte + ofs);
    *pWord          = val;
}

void setDwordByByteOffset(void *base, int ofs, uint32_t val)
{
    uint8_t *pByte     = (uint8_t *) base;
    uint32_t *pDword   = (uint32_t *) (pByte + ofs);
    *pDword         = val;
}
