// vim: expandtab shiftwidth=4 tabstop=4
#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <unistd.h>
#include <support.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ce_dd_prg.h"
#include "xbra.h"
#include "acsi.h"
#include "hdd_if.h"
#include "translated.h"
#include "gemdos.h"
#include "bios.h"
#include "main.h"
#include "mutex.h"
#include "sd_noob.h"
#include "serial.h"

/*
 * CosmosEx GEMDOS driver by Jookie, 2013-2016
 * GEMDOS hooks part (assembler and C) by MiKRO (Miro Kropacek), 2013
 */

// ------------------------------------------------------------------
// init and hooks part - MiKRO
typedef void  (*TrapHandlerPointer)( void );

extern void gemdos_handler( void );
extern TrapHandlerPointer old_gemdos_handler;
int32_t (*gemdos_table[256])( void* sp ) = { 0 };
int16_t useOldGDHandler = 0;								// 0: use new handlers, 1: use old handlers

extern void bios_handler( void );
extern TrapHandlerPointer old_bios_handler;
int32_t (*bios_table[256])( void* sp ) = { 0 };
int16_t useOldBiosHandler = 0;								// 0: use new handlers, 1: use old handlers
// ------------------------------------------------------------------
// CosmosEx and Gemdos part - Jookie
BYTE findDevice(void);

BYTE ce_findId(void);
void ce_initialize(void);
void getConfig(void);
BYTE setDateTime(void);
void showDateTime(void);
void showInt(int value, int length);
void showNetworkIPs(void);
void showIpAddress(BYTE *bfr);
void showAppVersion(void);
int getIntFromStr(const char *str, int len);

void set_longframe(void);
void setBootDriveAutomatic(void);
int  setBootDriveManual(int seconds);
void msleep(int ms);

void installCEPIcookie(void);

void possiblyFixCurrentDrive(void);

WORD dmaBuffer[DMA_BUFFER_SIZE/2];	/* declare as WORD buffer to force WORD alignment */

BYTE *pDmaBuffer;

BYTE deviceID;

BYTE commandShort[CMD_LENGTH_SHORT] = {         0, 'C', 'E', HOSTMOD_TRANSLATED_DISK, 0, 0};
BYTE commandLong [CMD_LENGTH_LONG]  = {0x1f, 0xA0, 'C', 'E', HOSTMOD_TRANSLATED_DISK, 0, 0, 0, 0, 0, 0, 0, 0};

BYTE *pDta;
BYTE tempDta[45];

WORD dtaBuffer[DTA_BUFFER_SIZE/2];
BYTE *pDtaBuffer;
BYTE fsnextIsForUs;

/* WORD ceDrives; definied in either harddrive_lowlevel.s or bios.c */
WORD ceMediach;
BYTE currentDrive;
WORD driveMap;
BYTE configDrive;

WORD tosVersion;
void getTOSversion(void);

BYTE setDate;
int year, month, day, hours, minutes, seconds;
BYTE netConfig[10];

extern DWORD _driverInstalled;              // when the driver is installed, set this variable to non-zero, otherwise the driver RAM will be freed by Mfree()
extern DWORD _runFromBootsector;			// flag meaning if we are running from TOS or bootsector
WORD trap_extra_offset=0;                   // Offset for GEMDOS/BIOS handler stack adjustment (should be 0 or 2)

WORD transDiskProtocolVersion;              // this will hold the protocol version from Main App
#define REQUIRED_TRANSLATEDDISK_VERSION     0x0101

volatile ScreenShots screenShots;           // screenshots config
void init_screencapture(void);

volatile mutex mtx;

#define COOKIEJARSIZE   16
DWORD ceCookieJar[2 * COOKIEJARSIZE];       // this might be the new cookie jar, if any doesn't exist, or is full

// ------------------------------------------------------------------
int main( int argc, char* argv[] )
{
    //initialize lock
    mutex_unlock(&mtx);

    // write some header out
    Clear_home();
    (void) Cconws("\33p[ CosmosEx disk driver  ]\r\n[ by Jookie 2013 - 2016 ]\r\n[        ver ");
    showAppVersion();
    (void) Cconws(" ]\33q\r\n\r\n");

    pDmaBuffer      = (BYTE *)dmaBuffer;

    Supexec(getTOSversion);
    
    // initialize internal stuff for Fsfirst and Fsnext
    fsnextIsForUs   = 0;
    pDtaBuffer      = (BYTE *)dtaBuffer;

    Supexec(set_longframe);

    if(SERIALDEBUG) { aux_sendString("\n\nCE_DD.PRG start\n"); }

//#define JUST_CEPI
    
    //--------------------------------
#ifndef JUST_CEPI    
    // don't install the driver is CTRL, ALT or SHIFT is pressed
    BYTE kbshift = Kbshift(-1);

    if((kbshift & 0x0f) != 0) {
        (void) Cconws("CTRL / ALT / SHIFT key pressed, not installing!\r\n" );
        sleep(2);
        return 0;
    }

    //--------------------------------
    // set boot drive and quit if requested by user -- but only if running from bootsector -- purpose: allow games to be launched from floppy
    if(_runFromBootsector) {
        int didSet;

        (void) Cconws("Set boot drive WITHOUT CE_DD: \33p" );
        didSet = setBootDriveManual(2);
        (void) Cconws("\33q\r\n" );

        if(didSet) {
            sleep(1);
            return 0;
        }
    }
    //--------------------------------
    // search for CosmosEx on ACSI bus
    BYTE found = Supexec(findDevice);

    if(!found) {                                            // not found? quit
        sleep(3);
        return 0;
    }

    // now set up the acsi command bytes so we don't have to deal with this one anymore
    commandShort[0] = (deviceID << 5);                      // cmd[0] = ACSI_id + TEST UNIT READY (0)
    commandLong [0] = (deviceID << 5) | 0x1f;               // cmd[0] = ACSI_id + ICD command marker (0x1f)

    //------------------------------
    // first check if we got the SD card and if it's SD NOOB, so if we have it, the ce_initialize() which will follow, will generate 'C' drive icon for SD card, too
    SDnoobPartition.verboseInit = TRUE;                     // verobe init - show messages
    Supexec(gotSDnoobCard);

    //------------------------------
    // tell the device to initialize
    Supexec(ce_initialize);

    // now init our internal vars
    pDta            = (BYTE *) &tempDta[0];                 // use this buffer as temporary one for DTA - just in case
    currentDrive    = Dgetdrv();                            // get the current drive from system
    driveMap        = Drvmap();                             // get the pre-installation drive map

    Supexec(getConfig);                                     // get translated disk configuration, including date and time

    if(transDiskProtocolVersion != REQUIRED_TRANSLATEDDISK_VERSION) {       // the current version of and required version of translated disk protocol don't match?
        (void) Cconws("\r\n\33pProtocol version mismatch !\33q\r\n" );
        (void) Cconws("Please use the newest version\r\n" );
        (void) Cconws("of \33pCE_DD.PRG\33q from config drive!\r\n" );
        (void) Cconws("\r\nDriver not installed!\r\n" );
        sleep(2);
        (void) Cnecin();
        return 0;
    }

    if(setDate) {                                           // now if we should set new date/time, then set it
        setDateTime();
        showDateTime();
    }

    showNetworkIPs();                                       // show IP addresses if possible

    Supexec(updateCeDrives);                                // update the ceDrives variable

    initFunctionTable();

    int i;
    for(i=0; i<MAX_FILES; i++) {
        initFileBuffer(i);                                  // init the file buffers
    }

    // either remove the old one or do nothing, old memory isn't released
    if( unhook_xbra( VEC_GEMDOS, 'CEDD' ) == 0L && unhook_xbra( VEC_BIOS, 'CEDD' ) == 0L ) {
        (void)Cconws( "\r\nDriver installed.\r\n" );
    } else {
        (void)Cconws( "\r\nDriver reinstalled, some memory was lost.\r\n" );
    }

    // and now place the new gemdos handler
    old_gemdos_handler  = Setexc( VEC_GEMDOS,   gemdos_handler );
    old_bios_handler    = Setexc( VEC_BIOS,     bios_handler );

    _driverInstalled = 1;                                   // mark that the driver was installed (and we don't want to Mfree() this RAM)

    //-------------------------------------
    // allow setting boot drive after driver being loaded -- purpose: allow to boot from USB drive / config drive / shared drive
    {
        int didSet;

        (void) Cconws("Set boot drive WITH    CE_DD: \33p" );
        didSet = setBootDriveManual(1);

        if(didSet) {                                        // if boot drive was set, wait a little
            msleep(500);
        } else {                                            // boot drive was not set, set to config drive if not pressing CTRL SHIFT
            kbshift = Kbshift(-1);

            if( (kbshift & (K_CTRL | K_LSHIFT)) == 0){      // not holding CTRL + SHIFT? Set boot drive
                Supexec(setBootDriveAutomatic);
            } else {
                (void) Cconws("\33q\r\nCTRL / SHIFT key pressed, not setting boot drive.\r\n" );
            }
        }
    }

    //-------------------------------------
    // if screenshots functionality was enabled
    if(screenShots.enabled) {
        (void) Cconws(">>> ScreenShots VBL installed. <<<\r\n" );
        Supexec(init_screencapture);
    }
#else
    // if just want CEPI installed
    hdd_if_select(IF_ACSI);    
#endif
    
    Supexec(installCEPIcookie);         // install the CEPI cookie

    //-------------------------------------
    // wait for a while so the user could read the message and quit
    sleep(1);

    if(SERIALDEBUG) { aux_sendString("\n\nCE_DD.PRG end\n"); }

    if(_runFromBootsector == 0) {       // if the prg was not run from boot sector, terminate and stay resident (execution ends here)
        Ptermres( 0x100 + _base->p_tlen + _base->p_dlen + _base->p_blen, 0 );
    }

    // if the prg was run from bootsector, we will return and the asm code will do rts
    return 0;
}

void getTOSversion(void)
{
    BYTE  *pSysBase     = (BYTE *) 0x000004F2;
    BYTE  *ppSysBase    = (BYTE *)  ((DWORD )  *pSysBase);                      // get pointer to TOS address
    tosVersion    = (WORD  ) *(( WORD *) (ppSysBase + 2));                // TOS +2: TOS version
}

// send INITIALIZE command to the CosmosEx device telling it to do all the stuff it needs at start
void ce_initialize(void)
{
	commandShort[0] = (deviceID << 5); 					                        // cmd[0] = ACSI_id + TEST UNIT READY (0)
	commandShort[4] = GD_CUSTOM_initialize;

    SET_WORD(pDmaBuffer + 0, tosVersion);                                       //  0, 1: store tos version

    WORD resolution     = Getrez();
    SET_WORD(pDmaBuffer + 2, resolution);                                       //  2, 3: store current screen resolution

    WORD drives         = Drvmap();
    SET_WORD(pDmaBuffer + 4, drives);                                           //  4, 5: store existing drives

    pDmaBuffer[6]       = SDnoobPartition.enabled;                              //  6   : SD card found, is SD NOOB card
    SET_DWORD(pDmaBuffer +  7, SDnoobPartition.sectorCount);                    //  7-10: SD NOOB partition size in sectors
    SET_DWORD(pDmaBuffer + 11, SDcard.SCapacity);                               // 11-14: SD card size in sectors
    
    (*hdIf.cmd)(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);     // issue the command and check the result
}

void getConfig(void)
{
    transDiskProtocolVersion = 0;                                               // no protocol version / failed

	commandShort[0] = (deviceID << 5); 					                        // cmd[0] = ACSI_id + TEST UNIT READY (0)
	commandShort[4] = GD_CUSTOM_getConfig;

	(*hdIf.cmd)(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);	    // issue the command and check the result

    if(!hdIf.success || hdIf.statusByte != OK) {                                // failed to get config?
        return;
    }

    configDrive = pDmaBuffer[4];                                                // store config drive letter for later use

    //------
    // get the date/time

    setDate = pDmaBuffer[5];

    year    = (((int) pDmaBuffer[7]) << 8) | ((int) pDmaBuffer[8]);
    month   = pDmaBuffer[9];
    day     = pDmaBuffer[10];

    hours   = pDmaBuffer[11];
    minutes = pDmaBuffer[12];
    seconds = pDmaBuffer[13];

    memcpy(netConfig, &pDmaBuffer[14], 10);

    transDiskProtocolVersion = GET_WORD(pDmaBuffer + 25);   // 25, 26: get the protocol version, so we can do a version matching / pairing

    screenShots.enabled = pDmaBuffer[27];                   // 27: screenshots VBL enabled?
    screenShots.take    = pDmaBuffer[28];                   // 28: take screenshot?
}

void setBootDriveAutomatic(void)
{
    // are there any other drives installed besides A+B? don't change boot drive if other drives are present
    // temporary disabled for TT as for some reason it gives out 4 bombs on driver's exit (on boot)
    if( (driveMap & 0xfffc)==0 && trap_extra_offset==0) {              // no other drives detected

        // do we have this configDrive?
        if( isOurDrive(configDrive, 0) ){
		    (void)Cconws( "Setting boot drive to " );
            (void)Cconout(configDrive + 'A');
		    (void)Cconws( ".\r\n" );

            CALL_OLD_GD_VOIDRET(Dsetdrv, configDrive);
        }
    }
}

BYTE setDateTime(void)
{
    WORD newDate, newTime;
    WORD newYear, newMonth;
    WORD newHour, newMinute, newSecond;
    BYTE res;

    //------------------
    // set new date
    newYear = year - 1980;
    newYear = newYear << 9;

    newMonth = month;
    newMonth = newMonth << 5;

    newDate = newYear | newMonth | (day & 0x1f);

    res = Tsetdate(newDate);

    if(res)                   // if some error, then failed
        return 0;

    //------------------
    // set new date
    newSecond   = ((seconds/2) & 0x1f);
    newMinute   = minutes & 0x3f;
    newMinute   = newMinute << 5;
    newHour     = hours & 0x1f;
    newHour     = newHour << 11;

    newTime = newHour | newMinute | newSecond;

    res = Tsettime(newTime);

    if(res)                   // if some error, then failed
        return 0;
    //------------------

    return 1;
}

void showDateTime(void)
{
    (void) Cconws("Date: ");
    showInt(year, 4);
    (void) Cconout('-');
    showInt(month, 2);
    (void) Cconout('-');
    showInt(day, 2);
    (void) Cconws(" (YYYY-MM-DD)\n\r");

    (void) Cconws("Time:   ");
    showInt(hours, 2);
    (void) Cconout(':');
    showInt(minutes, 2);
    (void) Cconout(':');
    showInt(seconds, 2);
    (void) Cconws(" (HH:MM:SS)\n\r");
}

void showInt(int value, int length)
{
    int negative;
    char tmp[10];
    char * p = tmp + sizeof(tmp);

    if(value < 0) {
        value = -value;
        negative = 1;
    } else {
        negative = 0;
    }
    *(--p) = '\0';    // null terminator
    do {
        // write all digits, starting at the right
        *(--p) = (value % 10) + '0';
        value = value / 10;
        length--;
    } while(value != 0 || length > 0);

    if(negative) {
        *(--p) = '-';
    }

    (void) Cconws(p);                     // write it out
}

void showNetworkIPs(void)
{
    if(netConfig[0] == 0 && netConfig[5] == 0) {                // no interface up?
        (void) Cconws("No working network interface.\n\r");
        return;
    }

    if(netConfig[0] == 1) {                                     // eth0 is up
        (void) Cconws("eth0 : ");
        showIpAddress(netConfig + 1);
        (void) Cconws("\n\r");
    }

    if(netConfig[5] == 1) {                                     // wlan0 is up
        (void) Cconws("wlan0: ");
        showIpAddress(netConfig + 6);
        (void) Cconws("\n\r");
    }
}

void showIpAddress(BYTE *bfr)
{
    showInt((int) bfr[0], -1);
    (void) Cconout('.');
    showInt((int) bfr[1], -1);
    (void) Cconout('.');
    showInt((int) bfr[2], -1);
    (void) Cconout('.');
    showInt((int) bfr[3], -1);
}

void showAppVersion(void)
{
    static const char months[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
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

// Adjust GEMDOS/BIOS trap handler offsets
void set_longframe(void)
{
	if(*_longframe != 0) {                // If != 0 then the cpu isn't 68000, so stack frame adjustment required
		trap_extra_offset=2;
    }
}

int setBootDriveManual(int seconds)
{
    int i, to;
    to = seconds * 5;

    for(i=0; i<to; i++) {
        DWORD gotChar;
        char  key, bootDrive;

        Cconout(' ');
        gotChar = Cconis();

        if(gotChar) {                                       // if some key pressed
            key = Cnecin();
            bootDrive = -1;

            if(key >= 'A' && key <= 'P') {                  // check if it's upper case letter
                bootDrive = key - 'A';
            }

            if(key >= 'a' && key <= 'p') {                  // check if it's lower case letter
                bootDrive = key - 'a';
            }

            if(bootDrive != -1) {                           // got a valid drive letter?
                (void) Cconws("\33q\r\nBoot drive: " );
                Cconout(bootDrive + 'A');
                (void) Cconws("\r\n" );

                CALL_OLD_GD_VOIDRET(Dsetdrv, bootDrive);    // set the drive to the selected drive
                sleep(2);
                return 1;
            }
        }

        msleep(200);                                        // no key, wait a while
    }

    (void) Cconws("\33q\r\n" );                             // when nothing (valid) was pressed, turn on inversion and go to new line
    return 0;
}

void possiblyFixCurrentDrive(void)
{
    DWORD bd = Dgetdrv();               // get current drive
    DWORD mask = (1 << bd);             // make mask out of it

    DWORD drives = Drvmap();            // get available drives

    if((drives & mask) != 0) {          // if the current drive exists, do nothing
        return;
    }

    // if the current drive does not exist, find existing drive and set it
    WORD i, drv;
    for(i=0; i<16; i++) {               // go from A: to P:
        drv = (1 << i);                 // create mask out of it

        if((drives & drv) != 0) {       // if drive exists
            Dsetdrv(i);                 // set it
            return;
        }

        Dsetdrv(0);                     // no valid drive found, set A: just in case
    }
}

BYTE installCookie(DWORD *cookieJar, DWORD key, DWORD value)
{
    DWORD cookieKey, cookieValue;

    int pos = 0;
    while(1) {                                  // go through the list of cookies
        cookieKey   = *cookieJar++;
        cookieValue = *cookieJar++;

        pos++;

        if(cookieKey != 0) {                    // cookie not empty? skip to next
            continue;
        }
        
        if(cookieKey == 0) {                    // if KEY is zero, it's LAST or EMPTY cookie
            if(cookieValue == pos) {            // it's LAST cookie, if the current position is the same as cookie value - we need to reallocate it (fail for now)
                return FALSE;
            }
        }

        // if got here, we're on EMPTY cookie which is not LAST
        cookieJar -= 2;                         // move 2 DWORDs back
        
        // next cookie - mark as last cookie
        cookieJar[2] = 0;                       // next cookie KEY      = 0 (last cookie)
        cookieJar[3] = cookieJar[1];            // next cookie VALUE    = cookie jar length (copied from current cookie)
        
        // this cookie - the store the new cookie value
        cookieJar[0] = key;
        cookieJar[1] = value;
        
        break;                                  // we're done
    }
    
    return TRUE;                                // success!
}

BYTE reallocateCookieJar(void)
{
    DWORD *cookieJarAddr    = (DWORD *) 0x05A0;
    DWORD *cookieJarOld     = (DWORD *) *cookieJarAddr;     // get pointer to current cookie jar

    DWORD *cookieJarNew     = &ceCookieJar[0];              // get pointer to new cookie jar
    
    DWORD cookieKey, cookieValue;

    int pos = 0;
    while(1) {                                  // go through the old list of cookies
        // read old cookie
        pos++;

        cookieKey   = *cookieJarOld++;          
        cookieValue = *cookieJarOld++;

        if(cookieKey == 0) {                    // no more cookies? quit, success
            break;
        }
        
        if(pos >= COOKIEJARSIZE) {              // if our new cookie jar is the same size (or smaller) as the old cookie jar, fail
            return FALSE;
        }
        
        // store the old cookie to new cookie jar
        cookieJarNew[0]  = cookieKey;
        cookieJarNew[1]  = cookieValue;
        cookieJarNew    += 2;
    }
    
    // store this to the last used cookie in new cookie jar
    cookieJarNew[0]     = 0;                        // this is the last cookie
    cookieJarNew[1]     = COOKIEJARSIZE;            // and this is the new cookie jar size
    
    *cookieJarAddr      = (DWORD) &ceCookieJar[0];  // update the pointer to cookie jar
    
    return TRUE;                                    // success!
}

void installCEPIcookie(void)
{
    // get address of cookie jar
    DWORD *cookieJarAddr    = (DWORD *) 0x05A0;
    DWORD *cookieJar        = (DWORD *) *cookieJarAddr;

    //------------
    // co cookie jar handling - allocation
    if(cookieJar == NULL) {                         // no cookie jar (on old TOS)?
        *cookieJarAddr  = (DWORD) &ceCookieJar[0];  // use our cookie jar as the new cookie jar
        cookieJar       = (DWORD *) *cookieJarAddr; // re-read what is the new cookie jar now
        
        ceCookieJar[0]  = 0;                        // key      = 0 (0th item is the last item)
        ceCookieJar[1]  = COOKIEJARSIZE;            // value    = size of cookie jar
    }
    //------------

    BYTE res;
    res = installCookie(cookieJar, 0x43455049, (DWORD) &hdIf);  // try to install CEPI cookie

    if(!res) {                                          // if failed to store cookie, that means the current cookie jar is empty
        res = reallocateCookieJar();
        
        if(!res) {                                      // failed to reallocate? quit with error
            (void) Cconws("Failed to reallocate cookie jar.\r\n");
            return;
        }
        
        cookieJar = (DWORD *) *cookieJarAddr;           // re-read the cookie jar position
    }
    
    res = installCookie(cookieJar, 0x43455049, (DWORD) &hdIf);  // try to install CEPI cookie
    
    if(!res) {
        (void) Cconws("Failed to install CEPI cookie.\r\n");
    } else {
        (void) Cconws("CEPI cookie installed.\r\n");
    }
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
