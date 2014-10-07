
/*********************************************************************/
/*                                                                   */
/*     STinG : API and IP kernel package                             */
/*                                                                   */
/*                                                                   */
/*      Version 1.0                      from 23. November 1996      */
/*                                                                   */
/*      Module for InterNet Control Message Protocol                 */
/*                                                                   */
/*********************************************************************/


#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <unistd.h>
#include <support.h>
#include <stdint.h>

#include <stdio.h>
#include <string.h>

#include "globdefs.h"


#define  M_YEAR    16
#define  M_MONTH   11
#define  M_DAY     23


void *  /* cdecl */  KRmalloc (int32 size);
void    /* cdecl */  KRfree (void *mem_block);

int16          ICMP_reply (uint8 type, uint8 code, IP_DGRAM *dgram, uint32 supplement);
int16   /* cdecl */  ICMP_send (uint32 dest, uint8 type, uint8 code, void *data, uint16 length);
int16   /* cdecl */  ICMP_handler (int16 /* cdecl */ handler (IP_DGRAM *), int16 flag);
void    /* cdecl */  ICMP_discard (IP_DGRAM *dgram);


extern PORT    my_port;
extern CONFIG  conf;
extern uint32  sting_clock;

LAYER       icmp_desc = {  "ICMP", "01.00", 0L, (M_YEAR << 9) | (M_MONTH << 5) | M_DAY, "Peter Rottengatter", 0, NULL, NULL  };
uint16      icmp_id = 0;

int16  ICMP_reply (uint8 type, uint8 code, IP_DGRAM *dgram, uint32 supple)
{


   return (TRUE);
}

int16  /* cdecl */  ICMP_send (uint32 dest, uint8 type, uint8 code, void *data, uint16 dat_length)
{

	return (E_NORMAL);
}

int16  /* cdecl */  ICMP_handler (int16 /* cdecl */ handler (IP_DGRAM *), int16 flag)
{


	return (FALSE);
}

void  /* cdecl */  ICMP_discard (IP_DGRAM *dgram)
{

}

