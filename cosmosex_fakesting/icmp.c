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

int16 ICMP_send (uint32 dest, uint8 type, uint8 code, void *data, uint16 dat_length)
{
    // first store command code
    commandShort[4] = NET_CMD_ICMP_SEND;
    commandShort[5] = 0;
    
    // then store the params in buffer
    BYTE *pBfr = pDmaBuffer;
    pBfr = storeDword   (pBfr, dest);
    pBfr = storeByte    (pBfr, type);
    pBfr = storeByte    (pBfr, code);
    pBfr = storeWord    (pBfr, dat_length);
    
    // TODO: send the data

    // send it to host
    WORD res = acsi_cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);

	if(res != OK) {                        										// if failed, return FALSE 
		return E_LOSTCARRIER;
	}

    // TODO: more handling here
    
	return (E_NORMAL);
}

int16 ICMP_handler (int16 handler (IP_DGRAM *), int16 flag)
{

	return (FALSE);
}

void ICMP_discard (IP_DGRAM *dgram)
{

}

