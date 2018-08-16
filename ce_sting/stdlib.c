    // vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
#include <mint/sysbind.h>
#include <mint/osbind.h>

#include "vbl.h"
#include "stdlib.h"

BYTE showHex_toLogNotScreen;

DWORD custom_getTicks(void)
{
    DWORD res;

    if(fromVbl) {                           // if called from vbl, call directly
        res = getTicks_fromSupervisor();
    } else {                                // if not called from vbl, call through Supexec()
        res = Supexec(getTicks_fromSupervisor);
    }

    return res;
}

DWORD getTicks_fromSupervisor(void)
{
	DWORD now;
	
	now = *HZ_200;
	return now;
}

BYTE *storeByte(BYTE *bfr, BYTE value)
{
    *bfr = (BYTE) value;                // store byte
    bfr++;                              // advance to next byte

    return bfr;                         // return the updated buffer address
}

BYTE *storeWord(BYTE *bfr, WORD value)
{
    *bfr = (BYTE) (value >> 8);         // store higher part
    bfr++;                              // advance to next byte

    *bfr = (BYTE) (value);              // store lower part
    bfr++;                              // advance to next byte

    return bfr;                         // return the updated buffer address
}

BYTE *storeDword(BYTE *bfr, DWORD value)
{
    *bfr = (BYTE) (value >> 24);        // store highest part
    bfr++;                              // advance to next byte

    *bfr = (BYTE) (value >> 16);        // store mid hi part
    bfr++;                              // advance to next byte

    *bfr = (BYTE) (value >>  8);        // store mid low part
    bfr++;                              // advance to next byte

    *bfr = (BYTE) (value);              // store lowest part
    bfr++;                              // advance to next byte

    return bfr;                         // return the updated buffer address
}

WORD getWord(BYTE *bfr)
{
    WORD val;

    val = ((WORD) bfr[0]) << 8;         // get upper part
    val = val | ((WORD) bfr[1]);        // get lower part

    return val;
}

DWORD getDword(BYTE *bfr)
{
    DWORD val;

    val =       ((DWORD) bfr[0]) << 24;
    val = val | ((DWORD) bfr[1]) << 16;
    val = val | ((DWORD) bfr[2]) <<  8;
    val = val | ((DWORD) bfr[3]);

    return val;
}

void showHexByte(int val)
{
    int hi, lo;
    char tmp[3];
    char table[16] = {"0123456789ABCDEF"};

    hi = (val >> 4) & 0x0f;;
    lo = (val     ) & 0x0f;

    tmp[0] = table[hi];
    tmp[1] = table[lo];
    tmp[2] = 0;

    if(showHex_toLogNotScreen) {            // showHex* functions output to log, not to screen?
        #ifdef DEBUG_STRING
        logStr(tmp);
        #endif
    } else {                                // showHex* functions ouptut to screen, not to log?
        (void) Cconws(tmp);
    }
}

void showHexWord(WORD val)
{
    BYTE a,b;
    a = val >>  8;
    b = val;

    showHexByte(a);
    showHexByte(b);
}

void showHexDword(DWORD val)
{
    BYTE a,b,c,d;
    a = val >> 24;
    b = val >> 16;
    c = val >>  8;
    d = val;

    showHexByte(a);
    showHexByte(b);
    showHexByte(c);
    showHexByte(d);
}

WORD getWordByByteOffset(void *base, int ofs)
{
    BYTE *pByte     = (BYTE *) base;
    WORD *pWord     = (WORD *) (pByte + ofs);
    WORD val        = *pWord;
    return val;
}

DWORD getDwordByByteOffset(void *base, int ofs)
{
    BYTE  *pByte    = (BYTE  *)  base;
    DWORD *pDword   = (DWORD *) (pByte + ofs);
    DWORD val       = *pDword;
    return val;
}

void *getVoidpByByteOffset(void *base, int ofs)
{
    void *p = (void *) getDwordByByteOffset(base, ofs);
    return p;
}


void setWordByByteOffset(void *base, int ofs, WORD val)
{
    BYTE *pByte  = (BYTE *)  base;
    WORD *pWord     = (WORD *) (pByte + ofs);
    *pWord          = val;
}

void setDwordByByteOffset(void *base, int ofs, DWORD val)
{
    BYTE *pByte     = (BYTE *) base;
    DWORD *pDword   = (DWORD *) (pByte + ofs);
    *pDword         = val;
}

//-------------------
#ifdef DEBUG_STRING
void logStr(char *str)
{
//    (void) Cconws(str);

    int len;
    int f = Fopen("C:\\ce_sting.log", 1);       // open file for writing

    if(f < 0) {                                 // if failed to open existing file
        f = Fcreate("C:\\ce_sting.log", 0);     // create file

        if(f < 0) {                             // if failed to create new file
            return;
        }
    }

    len = strlen(str);                          // get length

    Fseek(0, f, 2);                             // move to the end of file
    Fwrite(f, len, str);                        // write it to file
    Fclose(f);                                  // close it
}

void logBfr(BYTE *bfr, int len)
{
    int i;

    logStr("\n");

    for(i=0; i<len; i++) {
        showHexByte(bfr[i]);
        logStr(" ");
    }

    logStr("\n");
}
#endif
//-------------------
