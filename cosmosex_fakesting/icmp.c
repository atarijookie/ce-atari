//----------------------------------------
// CosmosEx fake STiNG - by Jookie, 2014
// Based on sources of original STiNG
//----------------------------------------

#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <support.h>
#include <stdint.h>
#include <stdio.h>

#include "globdefs.h"


#define  M_YEAR    16
#define  M_MONTH   11
#define  M_DAY     23

int16 ICMP_send     (uint32 dest, uint8 type, uint8 code, void *data, uint16 length);
int16 ICMP_handler  (int16  handler (IP_DGRAM *), int16 flag);
void  ICMP_discard  (IP_DGRAM *dgram);

extern PORT    my_port;
extern CONFIG  conf;
extern uint32  sting_clock;

LAYER       icmp_desc = {  "ICMP", "01.00", 0L, (M_YEAR << 9) | (M_MONTH << 5) | M_DAY, "Peter Rottengatter", 0, NULL, NULL  };
uint16      icmp_id = 0;

//---------------------
// ACSI / CosmosEx stuff
#include "acsi.h"
#include "ce_commands.h"
#include "stdlib.h"

extern BYTE deviceID;
extern BYTE commandShort[CMD_LENGTH_SHORT];
extern BYTE commandLong[CMD_LENGTH_LONG];
extern BYTE *pDmaBuffer;
//---------------------

#define MAX_ICMP_HANDLERS   16
DWORD icmpHandlers[MAX_ICMP_HANDLERS];

int16 ICMP_send (uint32 dest, uint8 type, uint8 code, void *data, uint16 dat_length)
{
    commandLong[5] = NET_CMD_ICMP_SEND_EVEN;                    // cmd[5]       = command code

    commandLong[6] = (BYTE) (dest >> 24);                       // cmd[6 .. 9]  = destination address
    commandLong[7] = (BYTE) (dest >> 16);
    commandLong[8] = (BYTE) (dest >>  8);
    commandLong[9] = (BYTE) (dest      );

    commandLong[10] = (BYTE) ((type << 3) | (code & 0x07));     // cmd[10]      = pack type and code together -- highest 5 bits are type, lowest 3 bits are code

    commandLong[11] = (BYTE) (dat_length >> 8);                 // cmd[11, 12]  = length
    commandLong[12] = (BYTE) (dat_length     );

    // prepare the command for buffer sending 
    BYTE *pBfr  = (BYTE *) data;
    DWORD dwBfr = (DWORD) data;

    if(dwBfr & 1) {                                 // buffer pointer is ODD
        commandLong[5] = NET_CMD_ICMP_SEND_ODD;     // cmd[5]       = command code -- sending from ODD address
        pBfr--;                                     // and pBfr is now EVEN, but we have to skip the 0th byte in host
    } else {                                        // buffer pointer is EVEN
        commandLong[5] = NET_CMD_ICMP_SEND_EVEN;    // cmd[5]       = command code -- sending from EVEN address
    }
    
    // calculate sector count
    WORD sectorCount = dat_length / 512;            // get number of sectors we need to send
    
    if((dat_length % 512) == 0) {                   // if the number of bytes is not multiple of 512, then we need to send one sector more
        sectorCount++;
    }
    
    // send it to host
    BYTE res = acsi_cmd(ACSI_WRITE, commandLong, CMD_LENGTH_LONG, pBfr, 1);

	if(res != OK) {                                 // if failed, return FALSE 
		return E_LOSTCARRIER;
	}

    return extendByteToWord(res);                   // return the status, possibly extended to int16
}

int16 ICMP_handler (int16 (* handler) (IP_DGRAM *), int16 flag)
{
    int i;
    int existing = -1, empty = -1;
    
    if(handler == NULL) {                                   // empty pointer? nothing to do
        return FALSE;
    }
    
    for(i=0; i<MAX_ICMP_HANDLERS; i++) {
        if(icmpHandlers[i] == (DWORD) handler) {            // if we got that handler, store it's index
            existing = i;
        }
        
        if(icmpHandlers[i] == 0 && empty == -1) {           // if this handler slot is empty and we don't have the empty index stored, store it
            empty = i;
        }
    }

    switch (flag) {
        case HNDLR_SET:                             // set handler?
        case HNDLR_FORCE:
            if(existing != -1) {                    // handler already exists? fail
                return FALSE;
            }
      
            icmpHandlers[empty] = (DWORD) handler;  // store handler
            return TRUE;
        //--------------------------------
        
        case HNDLR_REMOVE :                         // remove handler?
            if(existing == -1) {                    // but we don't have it? fail
                return FALSE;
            }
            
            icmpHandlers[existing] = (DWORD) NULL;  // clear handler
            return TRUE;
        //--------------------------------
        
        case HNDLR_QUERY :                          // check if this handler is registered?
            if(existing == -1) {
                return FALSE;
            } else {
                return TRUE;
            }
      }
      
    return FALSE;                                   // unknown request
}

void ICMP_discard (IP_DGRAM *dgram)
{
    // probably do nothing now...
    
}

