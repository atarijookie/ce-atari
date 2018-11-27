#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <unistd.h>
#include <gem.h>
#include <mt_gem.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "acsi.h"
#include "main.h"
#include "hostmoddefs.h"
#include "keys.h"
#include "defs.h"
#include "find_ce.h"
#include "hdd_if.h"
#include "CE_FDD.H"
#include "aes.h"

// ------------------------------------------------------------------
BYTE deviceID;
BYTE commandShort[CMD_LENGTH_SHORT] = {         0, 'C', 'E', HOSTMOD_FDD_SETUP, 0, 0};
BYTE commandLong [CMD_LENGTH_LONG]  = {0x1f, 0xA0, 'C', 'E', HOSTMOD_FDD_SETUP, 0, 0, 0, 0, 0, 0, 0, 0};

void createFullPath(char *fullPath, char *filePath, char *fileName);

char filePath[256], fileName[256];

BYTE *p64kBlock;
BYTE sectorCount;

BYTE *pBfrOrig;
BYTE *pBfr, *pBfrCnt;
BYTE *pDmaBuffer;

BYTE atariKeysToSingleByte(BYTE vkey, BYTE key);

BYTE loopForDownload(void);

BYTE gem_init(void);
void gem_deinit(void);
BYTE gem_floppySetup(void);
BYTE gem_imageDownload(void);

// ------------------------------------------------------------------
int main( int argc, char* argv[] )
{
	BYTE found;

    BYTE res = gem_init();  // initialize GEM stuff

    if(!res) {              // gem init failed? quit then
        return 0;
    }

    while(1) {
        res = gem_floppySetup();    // show and handle floppy setup via dialog

        if(res == KEY_F10) {        // should quit?
            break;
        }

        res = gem_imageDownload();    // show and handle floppy image download

        if(res == KEY_F10) {        // should quit?
            break;
        }
    }

    gem_deinit();                   // deinit GEM
    return 0;

    //-------------------------------------

	pBfrOrig = (BYTE *) Malloc(SIZE64K + 4);

	if(pBfrOrig == NULL) {
		(void) Cconws("\r\nMalloc failed!\r\n");
		sleep(3);
		return 0;
	}

	DWORD val = (DWORD) pBfrOrig;
	pBfr      = (BYTE *) ((val + 4) & 0xfffffffe);     				// create even pointer
    pBfrCnt   = pBfr - 2;											// this is previous pointer - size of WORD

    pDmaBuffer = pBfr;

    // init fileselector path
    strcpy(filePath, "C:\\*.*");
    memset(fileName, 0, 256);

    BYTE drive = getLowestDrive();                                  // get the lowest HDD letter and use it in the file selector
    filePath[0] = drive;

	// write some header out
	(void) Clear_home();
	(void) Cconws("\33p[ CosmosEx floppy setup ]\r\n[    by Jookie 2014     ]\33q\r\n\r\n");

	// search for CosmosEx on ACSI & SCSI bus
	found = Supexec(findDevice);

	if(!found) {								            // not found? quit
		sleep(3);
		return 0;
	}

    // now set up the acsi command bytes so we don't have to deal with this one anymore
    commandShort[0] = (deviceID << 5);          // cmd[0] = ACSI_id + TEST UNIT READY (0)
    commandLong[0]  = (deviceID << 5) | 0x1f;   // cmd[0] = ACSI_id + ICD command marker (0x1f)

	graf_mouse(M_OFF, 0);

    while(1) {
        BYTE key;

        key = gem_floppySetup();            // show and handle floppy setup via dialog

        if(key == KEY_F10) {                // should quit?
            break;
        }

        key = loopForDownload();

        if(key == KEY_F10) {                // should quit?
            break;
        }
    }

	graf_mouse(M_ON, 0);
	appl_exit();

	Mfree(pBfrOrig);

	return 0;
}

void intToStr(int val, char *str)
{
    int i3, i2, i1;
    i3 = (val / 100);               // 123 / 100 = 1
    i2 = (val % 100) / 10;          // (123 % 100) = 23, 23 / 10 = 2
    i1 = (val % 10);                // 123 % 10 = 3

    str[0] = i3 + '0';
    str[1] = i2 + '0';
    str[2] = i1 + '0';

    if(val < 100) {
        str[0] = ' ';
    }

    if(val < 10) {
        str[1] = ' ';
    }

    str[3] = 0;                     // terminating zero
}

BYTE getKey(void)
{
    DWORD scancode;
    BYTE key, vkey;

    scancode = Cnecin();                        /* get char form keyboard, no echo on screen */

    vkey    = (scancode >> 16)  & 0xff;
    key     =  scancode         & 0xff;

    key     = atariKeysToSingleByte(vkey, key);	/* transform BYTE pair into single BYTE */

    return key;
}

BYTE getKeyIfPossible(void)
{
    DWORD scancode;
    BYTE key, vkey, res;

    res = Cconis();                             // see if there's something waiting from keyboard

    if(res == 0) {                              // nothing waiting from keyboard?
        return 0;
    }

    scancode = Cnecin();                        // get char form keyboard, no echo on screen

    vkey = (scancode>>16) & 0xff;
    key  =  scancode      & 0xff;

    key = atariKeysToSingleByte(vkey, key);     // transform BYTE pair into single BYTE
    return key;
}

void removeLastPartUntilBackslash(char *str)
{
	int i, len;

	len = strlen(str);

	for(i=(len-1); i>= 0; i--) {
		if(str[i] == '\\') {
			break;
		}

		str[i] = 0;
	}
}

// make single ACSI read command by the params set in the commandLong buffer
BYTE ce_acsiReadCommandLong(void)
{
    memset(pBfr, 0, 512);               // clear the buffer

    (*hdIf.cmd)(ACSI_READ, commandLong, CMD_LENGTH_LONG, pBfr, sectorCount);   // issue the command and check the result 

    if(!hdIf.success) {
        return 0xff;
    }

    return hdIf.statusByte;
}

// make single ACSI read command by the params set in the commandShort buffer
BYTE ce_acsiReadCommand(void)
{
    commandShort[0] = (deviceID << 5); 											// cmd[0] = ACSI_id + TEST UNIT READY (0)

    memset(pBfr, 0, 512);              											// clear the buffer

    (*hdIf.cmd)(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pBfr, sectorCount);   // issue the command and check the result 

    if(!hdIf.success) {
        return 0xff;
    }

    return hdIf.statusByte;
}

BYTE ce_acsiWriteBlockCommand(void)
{
	commandShort[0] = (deviceID << 5); 											// cmd[0] = ACSI_id + TEST UNIT READY (0)

	(*hdIf.cmd)(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, p64kBlock, sectorCount);	// issue the command and check the result 

    if(!hdIf.success) {
        return 0xff;
    }

	return hdIf.statusByte;
}

BYTE getLowestDrive(void)
{
    BYTE i;
    DWORD drvs = Drvmap();
    DWORD mask;

    for(i=2; i<16; i++) {                                                       // go through the available drives
        mask = (1 << i);

        if((drvs & mask) != 0) {                                                // drive is available?
            return ('A' + i);
        }
    }

    return 'A';
}

void showError(const char *error)
{
    (void) Clear_home();
    (void) Cconws(error);
    Cnecin();
}

void showComError(void)
{
    showError("Error in CosmosEx communication!\r\n");
}

void createFullPath(char *fullPath, char *filePath, char *fileName)
{
    strcpy(fullPath, filePath);

	removeLastPartUntilBackslash(fullPath);				// remove the search wildcards from the end

	if(strlen(fullPath) > 0) {
		if(fullPath[ strlen(fullPath) - 1] != '\\') {	// if the string doesn't end with backslash, add it
			strcat(fullPath, "\\");
		}
	}

    strcat(fullPath, fileName);							// add the filename
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

BYTE gem_init(void)
{
    //-------------------------------------
    int16_t work_in[11], i, work_out[64];
    gl_apid = appl_init();

    if (gl_apid == -1) {
        (void) Cconws("appl_init() failed\r\n");
        return FALSE;
    }

    wind_update(BEG_UPDATE);
    graf_mouse(HOURGLASS, 0);

    int16_t res = rsrc_load("CE_FDD.RSC");
    graf_mouse(ARROW, 0);

    if(!res) {
        (void) Cconws("rsrc_load() failed\r\n");
        return FALSE;
    }

    for(i=0; i<10; i++) {
        work_in[i]= i;
    }

    work_in[10] = 2;

    int16_t gem_handle, vdi_handle;
    int16_t gl_wchar, gl_hchar, gl_wbox, gl_hbox;

    gem_handle = graf_handle(&gl_wchar, &gl_hchar, &gl_wbox, &gl_hbox);
    vdi_handle = gem_handle;
    v_opnvwk(work_in, &vdi_handle, work_out);

    if (vdi_handle == 0) {
        (void) Cconws("v_opnvwk() failed\r\n");
        return FALSE;
    }

    return TRUE;
}

void gem_deinit(void)
{
    rsrc_free();            // free resource from memory
}
