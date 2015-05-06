#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/ostruct.h>
#include <support.h>

#include <stdio.h>
#include "stdlib.h"
#include "out.h"

void byteToHex ( BYTE val, char *bfr);
void wordToHex ( WORD val, char *bfr);
void dwordToHex(DWORD val, char *bfr);

int   bufferSize;
char *buffer;

char *bfrCur;
int   curSize;

extern int  drive;
extern WORD tosVersion;

void out_sw(char *str1, WORD w1)
{
    char tmp[16];
    
    (void) Cconws(str1);
           wordToHex(w1, tmp);
    (void) Cconws(tmp);
    (void) Cconws("\n\r");
    
    appendToBuf(str1);
    appendToBuf(tmp);
    appendToBuf("\n\r");
}

void out_sc(char *str1, char c)
{
    char tmp[16];
    
    (void) Cconws(str1);
    (void) Cconout(c);
    (void) Cconws("\n\r");

    tmp[0] = c;
    tmp[1] = 0;
    
    appendToBuf(str1);
    appendToBuf(tmp);
    appendToBuf("\n\r");
}

void appendToBuf(char *str)
{
    if(!bfrCur) {                               // no buffer? quit
        return;
    }

    int len = strlen(str);
    
    if((curSize + len + 1) > bufferSize) {      // would go out of buffer? quit
        return;
    }
    
    strcpy(bfrCur, str);                        // copy in the data
    
    bfrCur  += len;                             // move the pointer, update the counter
    curSize += len;
}

#define BUFFER_SIZE     (64 * 1024)
void initBuffer(void)
{
    bufferSize  = 0;
    buffer      = NULL;

    curSize     = 0;
    bfrCur      = NULL;

    buffer      = (char *) Malloc(BUFFER_SIZE); // try to malloc
    
    if(buffer != NULL) {                        // on success - store size and pointer
        bufferSize  = BUFFER_SIZE;
        bfrCur      = buffer;
    } else {
        (void) Cconws("Failed to allocate buffer for results!\r\n");
    }
}

void writeBufferToFile(void)
{
    if(!buffer) {
        return;
    }

    char path[32] = {"X:\\cet_TTTT.txt"};
    path[0] = 'A' + drive;                      // place drive letter
    
    char tmp[16];
    wordToHex(tosVersion, tmp);
    memcpy(path + 7, tmp, 4);                   // place TOS version
    
    int f = Fcreate(path, 0);                   // create file
    if(f < 0) {
        (void) Cconws("Failed to create log file.\r\n");
        return;
    }
    
    Fwrite(f, curSize, buffer);
    Fclose(f);
}

void deinitBuffer(void)
{
    if(!buffer) {
        return;
    }
    
    Mfree(buffer);
    
    buffer      = NULL;
    bfrCur      = NULL;
    bufferSize  = 0;
    curSize     = 0;
}

void byteToHex( BYTE val, char *bfr)
{
    int hi, lo;
    char table[16] = {"0123456789ABCDEF"};
    
    hi = (val >> 4) & 0x0f;;
    lo = (val     ) & 0x0f;

    bfr[0] = table[hi];
    bfr[1] = table[lo];
    bfr[2] = 0;
}

void wordToHex( WORD val, char *bfr)
{
    BYTE a,b;
    a = val >>  8;
    b = val;
    
    byteToHex(a, bfr + 0);
    byteToHex(b, bfr + 2);
}

void dwordToHex(DWORD val, char *bfr)
{
    BYTE a,b,c,d;
    a = val >> 24;
    b = val >> 16;
    c = val >>  8;
    d = val;
    
    byteToHex(a, bfr + 0);
    byteToHex(b, bfr + 2);
    byteToHex(c, bfr + 4);
    byteToHex(d, bfr + 6);    
}
