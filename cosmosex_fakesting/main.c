
/*********************************************************************/
/*                                                                   */
/*     STinG : API and IP kernel package                             */
/*                                                                   */
/*                                                                   */
/*      Version 1.0                      from 23. November 1996      */
/*                                                                   */
/*      Module for Installation, Config Strings, *.STX Loading       */
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
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "globdefs.h"

#define  MAX_SEMAPHOR    64


long           init_cookie (void);

uint16         lock_exec (uint16 status);

void           install (void);
int16          KRinitialize (int32 size);
void *  /* cdecl */  KRmalloc (int32 size);
void    /* cdecl */  KRfree (void *mem_block);

int16   /* cdecl */  routing_table (void);

void           get_path (void);
long           get_boot_drv (void);
int16          compare (char string_1[], char string_2[], int16 number);
int16          init_cfg (char filename[]);
int16   /* cdecl */  setvstr (char name[], char value[]);
long           do_setvstr (void *array);
char *  /* cdecl */  getvstr (char name[]);
void           load_stx (void);


extern CONFIG  conf;
extern void    *memory;

char  sting_path[245], semaphors[MAX_SEMAPHOR];

extern int32 _pgmsize;

#define CONFIGVAR_COUNT		33
char *configvar_names[CONFIGVAR_COUNT]	= {"ALLOCMEM", "ACTIVATE", "THREADING", "FRAG_TTL", "ICMP_GMT", "ICMP_AD", "ICMP_FLAG", "UDP_PORT", "UDP_ICMP", "TCP_PORT", "MSS", "RCV_WND", "DEF_RTT", "DEF_TTL", "TCP_ICMP", "DOMAIN", "DNS_CACHE", "DNS_SAVE", "USERNAME", "HOSTNAME", "FULLNAME", "NAMESERVER", "DIALER", "LOGIN_BATCH", "EMAIL", "SMTP_HOST", "MAILER", "POP_HOST", "POP_USERNAME", "POP_PASSWORD", "TIME_ZONE", "TIME_SUMMER", "TIME_SERVER"};
char *configvar_values[CONFIGVAR_COUNT]	= {"100000", "TRUE", "50", "60", "-60", "10", "0", "1024", "1", "1024", "1460", "10000", "1500", "64", "1", "sting.org", "64", "TRUE", "your_username", "your_hostname", "your_fullname", "", "127.0.0.1", "C:\\DIALER\\LOGIN.BAT", "your_mail_address", "your_mail_server", "your_mail_server", "your_mail_server", "your_mail_username", "your_mail_password", "+60", "03.26.10.29", "time.demon.co.uk"};

int main()
{
   int   count;
   char  def_conf[255];

   (void) Cconws("\n\r\033p  *** Fake STinG TCP/IP InterNet Connection Layer ***  \033q");

   for (count = 0; count < MAX_SEMAPHOR; count++)
        semaphors[count] = 0;

   switch (init_cfg (def_conf)) {
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

   Ptermres (_pgmsize, 0);
   return 0;
 }

long  get_boot_drv()
{
   unsigned  int  *_bootdev = (void *) 0x446L;

   return ((long) ('A' + *_bootdev));
 }


int16  compare (char *string_1, char *string_2, int16 number)
{
   int16  count;

   for (count = 0; count < number; count++) {
        if (toupper (string_1[count]) != toupper (string_2[count]))
             return (FALSE);
        if (! string_1[count] || ! string_2[count])
             return (FALSE);
      }

   return (TRUE);
 }

int16  init_cfg (char *fname)
{
	if(KRinitialize(100000) < 0) {				// ALLOCMEM
		return (-3);
	}

	return (0);
}

int16  /* cdecl */  setvstr (char  *name, char  *value)
{

   return (TRUE);
 }

char *  /* cdecl */  getvstr (char *name)
{
	int i;
	
	for(i=0; i<CONFIGVAR_COUNT; i++) {					// search the values
		if(strcmp(name, configvar_names[i]) == 0) {		// if name found
			return configvar_values[i];					// return value
		}
	}
	
	return "0";
}
