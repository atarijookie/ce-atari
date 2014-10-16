//----------------------------------------
// CosmosEx fake STiNG - by Jookie, 2014
// Based on sources of original STiNG
//----------------------------------------

#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <unistd.h>
#include <support.h>
#include <stdint.h>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include <gem.h>

#include "globdefs.h"
#include "acsi.h"
#include "ce_commands.h"

long    init_cookie (void);
void    install (void);
int16   init_cfg (void);

int16   KRinitialize (int32 size);
void *  KRmalloc (int32 size);
void    KRfree (void *mem_block);

int16   setvstr (char name[], char value[]);
char *  getvstr (char name[]);

extern CONFIG  conf;
extern void    *memory;

char  semaphors[MAX_SEMAPHOR];

extern int32 _pgmsize;

#define CONFIGVAR_COUNT		33
char *configvar_names[CONFIGVAR_COUNT]	= {"ALLOCMEM", "ACTIVATE", "THREADING", "FRAG_TTL", "ICMP_GMT", "ICMP_AD", "ICMP_FLAG", "UDP_PORT", "UDP_ICMP", "TCP_PORT", "MSS", "RCV_WND", "DEF_RTT", "DEF_TTL", "TCP_ICMP", "DOMAIN", "DNS_CACHE", "DNS_SAVE", "USERNAME", "HOSTNAME", "FULLNAME", "NAMESERVER", "DIALER", "LOGIN_BATCH", "EMAIL", "SMTP_HOST", "MAILER", "POP_HOST", "POP_USERNAME", "POP_PASSWORD", "TIME_ZONE", "TIME_SUMMER", "TIME_SERVER"};
char *configvar_values[CONFIGVAR_COUNT]	= {"100000", "TRUE", "50", "60", "-60", "10", "0", "1024", "1", "1024", "1460", "10000", "1500", "64", "1", "sting.org", "64", "TRUE", "your_username", "your_hostname", "your_fullname", "", "127.0.0.1", "C:\\DIALER\\LOGIN.BAT", "your_mail_address", "your_mail_server", "your_mail_server", "your_mail_server", "your_mail_username", "your_mail_password", "+60", "03.26.10.29", "time.demon.co.uk"};

//---------------------------------------
// ACSI and CosmosEx stuff
BYTE deviceID;
BYTE commandShort[CMD_LENGTH_SHORT]	= {      0x00, 'C', 'E', HOSTMOD_NETWORK_ADAPTER, 0, 0};
BYTE commandLong[CMD_LENGTH_LONG]	= {0x1f, 0xA0, 'C', 'E', HOSTMOD_NETWORK_ADAPTER, 0, 0, 0, 0, 0, 0, 0, 0};

BYTE dmaBuffer[512 + 2]; 
BYTE *pDmaBuffer;

BYTE ce_findId(void); 
//---------------------------------------

extern uint32 localIP;

//---------------------------------------

int main()
{
   int   count;
   char  def_conf[255];

   (void) Cconws("\n\r\033p  *** Fake STinG TCP/IP InterNet Connection Layer ***  \033q");

   	// create buffer pointer to even address
	pDmaBuffer = &dmaBuffer[2];
	pDmaBuffer = (BYTE *) (((DWORD) pDmaBuffer) & 0xfffffffe);  // remove odd bit if the address was odd
    
	BYTE found = ce_findId();                                   // try to find the CosmosEx device on ACSI bus

    if(!found) {								                // not found? quit
        sleep(3);
        return 0;
    }
    
    commandShort[0] = (deviceID << 5); 					        // cmd[0] = ACSI_id + TEST UNIT READY (0)
    commandLong[0]  = (deviceID << 5) | 0x1f;			        // cmd[0] = ACSI_id + ICD command marker (0x1f)
   
   for (count = 0; count < MAX_SEMAPHOR; count++)
        semaphors[count] = 0;

   switch (init_cfg()) {
      case -3 :
        (void) Cconws ("Could not allocate enough memory ! No installation ...");
        return 0;
      case -2 :
        (void) Cconws ("ALLOCMEM must be at least 1024 bytes ! No installation ...");
        return 0;
      case -1 :
        (void) Cconws ("Problem finding/reading DEFAULT.CFG ! No installation ...");
        return 0;
      }

	if (Supexec (init_cookie) < 0) {
        (void) Cconws ("STinG already installed ! No installation ...");
        if (memory)   Mfree (memory);
        return 0;
	}

   install();

   strcpy (def_conf, "STinG version ");   strcat (def_conf, TCP_DRIVER_VERSION);
   strcat (def_conf, " (");               strcat (def_conf, STX_LAYER_VERSION);
   strcat (def_conf, ") installed ...");
   (void) Cconws(def_conf);

   appl_init();                                 // init gem
   
   Ptermres (_pgmsize, 0);
   return 0;
 }

int16  init_cfg (void)
{
	if(KRinitialize(100000) < 0) {				// ALLOCMEM
		return (-3);
	}

	return (0);
}

int16 setvstr (char  *name, char  *value)
{

   return (TRUE);
}

char *getvstr (char *name)
{
	int i;
	
	for(i=0; i<CONFIGVAR_COUNT; i++) {					// search the values
		if(strcmp(name, configvar_names[i]) == 0) {		// if name found
			return configvar_values[i];					// return value
		}
	}
	
	return "0";
}

// send an IDENTIFY command to specified ACSI ID and check if the result is as expected
BYTE ce_identify(void)
{
	WORD res;
  
	commandShort[0] = (deviceID << 5); 											// cmd[0] = ACSI_id + TEST UNIT READY (0)	
	commandShort[4] = NET_CMD_IDENTIFY;
  
	memset(pDmaBuffer, 0, 512);              									// clear the buffer 

	res = acsi_cmd(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);	// issue the command and check the result 

	if(res != OK) {                        										// if failed, return FALSE 
		return 0;
	}

	if(strncmp((char *) pDmaBuffer, "CosmosEx network module", 23) != 0) {		// the identity string doesn't match? 
		return 0;
	}
	
	return 1;                             										// success 
}

// this function scans the ACSI bus for any active CosmosEx translated drive
BYTE ce_findId(void)
{
	char bfr[2], res, i;

	bfr[1] = 0;
	deviceID = 0;
	
	(void) Cconws("Looking for CosmosEx: ");

	for(i=0; i<8; i++) {
		bfr[0] = i + '0';
		(void) Cconws(bfr);

		deviceID = i;									// store the tested ACSI ID 
		res = Supexec(ce_identify);  					// try to read the IDENTITY string 
		
		if(res == 1) {                           		// if found the CosmosEx 
			(void) Cconws("\r\nCosmosEx found on ACSI ID: ");
			bfr[0] = i + '0';
			(void) Cconws(bfr);
			(void) Cconws("\r\n");

			return 1;
		}
	}

	// if not found 
    (void) Cconws("\r\nCosmosEx not found on ACSI bus, not installing driver.");
	return 0;
}
