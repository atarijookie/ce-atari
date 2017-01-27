// vim: expandtab shiftwidth=4 tabstop=4
#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <support.h>

#include <stdint.h>
#include <stdio.h>

#include "stdlib.h"
#include "hdd_if.h"
#include "translated.h"
#include "main.h"

// ------------------------------------------------------------------
WORD tosVersion;
void getTosVersion(void);

THDif *hdIf;
void getCE_API(void);

WORD dmaBuffer[DMA_BUFFER_SIZE/2];  // declare as WORD buffer to force WORD alignment
BYTE *pDmaBuffer;

BYTE commandShort[CMD_LENGTH_SHORT] = {      0, 'C', 'E', HOSTMOD_TRANSLATED_DISK, 0, 0};
BYTE commandLong [CMD_LENGTH_LONG]  = {0x1f, 0, 'C', 'E', HOSTMOD_TRANSLATED_DISK, 0, 0, 0, 0, 0, 0, 0, 0};

// ------------------------------------------------------------------
int main( int argc, char* argv[] )
{
    // write some header out
    Clear_home();
    //                |                                        |
    (void) Cconws("\33p   [ CosmosEx SD NOOB configurator ]    \33q\r\n\r\n");

    //            |                                        |
    (void) Cconws("This tool is for inexperienced users and\r\n");
    (void) Cconws("it will automatically partition your    \r\n");
    (void) Cconws("SD card to maximum size allowed on your \r\n");
    (void) Cconws("TOS version, making it DOS compatible   \r\n");
    (void) Cconws("and accessible on ACSI bus.             \r\n");
    (void) Cconws("\r\n");
    (void) Cconws("If you know how to do this by yourself, \r\n");
    (void) Cconws("then you might achieve better results   \r\n");
    (void) Cconws("doing so than with this automatic tool. \r\n");
    (void) Cconws("\r\n");
    (void) Cconws("Press \33p[Q]\33q to quit.\r\n");
    (void) Cconws("Press \33p[C]\33q to continue.\r\n");

    while(1) {
        BYTE key = Cnecin();

        if(key == 'q' || key == 'Q') {  // quit?
            return 0;
        }

        if(key == 'c' || key == 'C') {  // continue?
            break;
        }
    }

    //-------------
    // if user decided to continue...
    Clear_home();
    Supexec(getTosVersion);             // find out TOS version

    //            |                                        |
    // show TOS version
    (void) Cconws("Your TOS version      : ");
    showInt((tosVersion >> 16) & 0xff, 1);
    (void) Cconws(".");
    showInt((tosVersion >>  8) & 0xff, 1);
    showInt((tosVersion      ) & 0xff, 1);
    (void) Cconws("\r\n");

    // show maximum partition size
    (void) Cconws("Maximum partition size: ");

    WORD partitionSizeMB;

    if(tosVersion <= 0x0102) {          // TOS 1.02 and older
        partitionSizeMB =  256;
        (void) Cconws("256 MB\r\n");
    } else if(tosVersion < 0x0400) {    // TOS 1.04 - 3.0x
        partitionSizeMB =  512;
        (void) Cconws("512 MB\r\n");
    } else {                            // TOS 4.0x
        partitionSizeMB = 1024;
        (void) Cconws("1024 MB\r\n");
    } 

    //-------------
    // find CE_DD API in cookie jar
    Supexec(getCE_API);

    (void) Cconws("CosmosEx DD API       : ");
    if(hdIf) {                                  // if CE_DD API was found, good
        (void) Cconws("found\r\n");
    } else {                                    // if CE_DD API wasn't found, fail
        (void) Cconws("not found\r\n\r\n");
        (void) Cconws("\33pPlease run CE_DD.PRG before this tool!\33q\r\n");
        (void) Cconws("Press any key to terminate...\r\n");

        Cnecin();
        return 0;
    }

    //-------------
    // TODO: use solo command to get if SD card is inserted, and its capacity, if not, loop until it's inserted

    //-------------
    // TODO: get IDs from CE, find out if SD is enabled on ACSI BUS:
    // if SD is enabled, do nothing
    // if SD is not enabled, do ACSI bus scan, check for free ACSI IDs (so it wouldn't colide with any existing HDD or CE), set this ACSI ID to SD card

    //-------------
    // TODO: read boot sector from SD card, if if contains some other driver, warn user

    //-------------
    // TODO: warn user that if he will proceed, he will loose data

    //-------------
    // TODO: if continuing, write boot sector and everything needed for partitioning

    //-------------
    // TODO: show message that we're done and we need to reset the ST to apply new settings

    //-------------

    return 0;
}

//--------------------------------------------
void getTosVersion(void)
{
    BYTE  *pSysBase     = (BYTE *) 0x000004F2;
    BYTE  *ppSysBase    = (BYTE *)  ((DWORD )  *pSysBase);          // get pointer to TOS address

    tosVersion          = (WORD  ) *(( WORD *) (ppSysBase + 2));    // TOS +2: TOS version
}

//--------------------------------------------
void getCE_API(void)
{
    // get address of cookie jar
    DWORD *cookieJarAddr    = (DWORD *) 0x05A0;
    DWORD *cookieJar        = (DWORD *) *cookieJarAddr;

    hdIf = NULL;

    if(cookieJar == 0) {                        // no cookie jar? it's an old ST and CE_DD wasn't loaded 
        return;
    }

    DWORD cookieKey, cookieValue;

    while(1) {                                  // go through the list of cookies
        cookieKey   = *cookieJar++;
        cookieValue = *cookieJar++;

        if(cookieKey == 0) {                    // end of cookie list? then cookie not found
            return;
        }

        if(cookieKey == 0x43455049) {           // is it 'CEPI' key? found it, store it, quit
            hdIf = (THDif *) cookieValue;
            return;
        }
    }
}
//--------------------------------------------
