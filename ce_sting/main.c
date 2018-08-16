// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
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
#include <ctype.h>

#include "../ce_hdd_if/stdlib.h"
#include "../ce_hdd_if/hdd_if.h"

#include "globdefs.h"
#include "ce_commands.h"
#include "con_man.h"
#include "stdlib.h"
#include "setup.h"
#include "vbl.h"

#define REQUIRED_NETADAPTER_VERSION     0x0100

void showAppVersion(void);
int  getIntFromStr(const char *str, int len);
void showInt(int value, int length);

long    init_cookie (void);
void    install (void);
int16   init_cfg (void);

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

BYTE dmaBuffer[DMA_BUFFER_SIZE + 2];
BYTE *pDmaBuffer;

//---------------------------------------

DWORD localIP;
WORD  requiredVersion;

void initJumpTable(void);

//---------------------------------------
extern BYTE showHex_toLogNotScreen;

int main(void)
{
    int   count;

    showHex_toLogNotScreen = 0;                                 // showHex* to screen
    initJumpTable();                                            // fill the jump table with addresses of functions

    (void) Cconws("\n\r\033p|    Fake STinG for CosmosEx    |\033q");
    (void) Cconws("\n\r\033p|   by Jookie, ver: ");
    showAppVersion();
    (void) Cconws("  |\033q\n\r");

    init_con_info();                                            // init connection info structs

   	// create buffer pointer to even address
    pDmaBuffer = &dmaBuffer[2];
    pDmaBuffer = (BYTE *) (((DWORD) pDmaBuffer) & 0xfffffffe);  // remove odd bit if the address was odd

    // search for CosmosEx on ACSI & SCSI bus
    deviceID = Supexec(findDevice);

    if(deviceID == DEVICE_NOT_FOUND) {
        sleep(3);
        return 0;
    }

    if(requiredVersion != REQUIRED_NETADAPTER_VERSION) {
        (void) Cconws("\r\n\33pProtocol version mismatch !\33q\r\n" );
        (void) Cconws("Please use the newest version\r\n" );
        (void) Cconws("of \33pCE_STING.PRG\33q from config drive!\r\n" );
        (void) Cconws("\r\nDriver not installed!\r\n" );
        sleep(3);
        return 0;
    }

    commandShort[0] = (deviceID << 5); 					        // cmd[0] = ACSI_id + TEST UNIT READY (0)
    commandLong[0]  = (deviceID << 5) | 0x1f;			        // cmd[0] = ACSI_id + ICD command marker (0x1f)

    for (count = 0; count < MAX_SEMAPHOR; count++) {
        semaphors[count] = 0;
    }

    switch (init_cfg()) {
        case -3 :   (void) Cconws ("Could not allocate enough memory ! No installation ...");       return 0;
        case -2 :   (void) Cconws ("ALLOCMEM must be at least 1024 bytes ! No installation ...");   return 0;
        case -1 :   (void) Cconws ("Problem finding/reading DEFAULT.CFG ! No installation ...");    return 0;
    }

	if (Supexec (init_cookie) < 0) {
        (void) Cconws ("Already installed, just quitting.");
        if (memory) {
            Mfree (memory);
        }

        sleep(2);
        return 0;
	}

    install();

    /* do not install vbl handler now, wait until we need it :
       when an ICMP handler is installed. All other STing functions
       are based on polling
    Supexec(install_vbl);
    vblEnabled = 1;
    */

    (void) Cconws("Driver was installed...");

    showHex_toLogNotScreen = 1;                                 // showHex* to log

//    appl_init();                                                // init gem
    sleep(2);

    Ptermres (_pgmsize, 0);
    return 0;
 }

int16 init_cfg (void)
{
	if(KRinitialize(100000) < 0) {				// ALLOCMEM
		return (-3);
	}

	return (0);
}

int16 setvstr (char  *name, char  *value)
{

    return TRUE;
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

void showAppVersion(void)
{
    char months[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    char const *buildDate = __DATE__;

    int year = 0, month = 0, day = 0;
    int i;
    for(i=0; i<12; i++) {
        if(strncmp(months[i], buildDate, 3) == 0) {
            month = i + 1;
            break;
        }
    }

    day     = getIntFromStr(buildDate + 4, 2);
    year    = getIntFromStr(buildDate + 7, 4);

    if(day > 0 && month > 0 && year > 0) {
        showInt(year, 4);
        (void) Cconout('-');
        showInt(month, 2);
        (void) Cconout('-');
        showInt(day, 2);
    } else {
        (void) Cconws("YYYY-MM-DD");
    }
}

int getIntFromStr(const char *str, int len)
{
    int i;
    int val = 0;

    for(i=0; i<len; i++) {
        int digit;

        if(str[i] >= '0' && str[i] <= '9') {
            digit = str[i] - '0';
        } else {
            digit = 0;
        }

        val *= 10;
        val += digit;
    }

    return val;
}

void logMsg(char *logMsg)
{
//    if(showLogs) {
//        (void) Cconws(logMsg);
//    }
}

void logMsgProgress(DWORD current, DWORD total)
{
//    if(!showLogs) {
//        return;
//    }

//    (void) Cconws("Progress: ");
//    showHexDword(current);
//    (void) Cconws(" out of ");
//    showHexDword(total);
//    (void) Cconws("\n\r");
}

