#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <unistd.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "serial.h"

void aux_sendChar(char c)
{
    while(1) {                          // wait indefinitelly until ready
        if(Bcostat(DEV_AUX) == 0) {     // not read to send? wait some more
            continue;
        }

        break;                          // can send, don't wait anymore
    }

    Bconout(DEV_AUX, c);                // send character
}

void aux_sendString(char *str)
{
    while(*str != 0) {                  // while not string terminator
        if(Bcostat(DEV_AUX) == 0) {     // not read to send? wait some more
            continue;
        }
    
        Bconout(DEV_AUX, *str);         // send character
        str++;
    }
}

void aux_hexNibble(BYTE val)
{
    int nibble;
    char table[16] = {"0123456789ABCDEF"};
    
    nibble = val & 0x0f;

    aux_sendChar(table[nibble]);
}

void aux_hexByte(BYTE val)
{
    int hi, lo;
    char table[16] = {"0123456789ABCDEF"};
    
    hi = (val >> 4) & 0x0f;;
    lo = (val     ) & 0x0f;

    aux_sendChar(table[hi]);
    aux_sendChar(table[lo]);
}

void aux_hexWord(WORD val)
{
    aux_hexByte((BYTE) (val >>  8));
    aux_hexByte((BYTE)  val);
}

void aux_hexDword(DWORD val)
{
    aux_hexByte((BYTE) (val >> 24));
    aux_hexByte((BYTE) (val >> 16));
    aux_hexByte((BYTE) (val >>  8));
    aux_hexByte((BYTE)  val);
}
