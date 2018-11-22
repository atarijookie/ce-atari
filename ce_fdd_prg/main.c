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
#include "FDD.H"

// ------------------------------------------------------------------
BYTE deviceID;
BYTE commandShort[CMD_LENGTH_SHORT]	= {	0, 'C', 'E', HOSTMOD_FDD_SETUP, 0, 0};

void createFullPath(char *fullPath, char *filePath, char *fileName);

char filePath[256], fileName[256];

BYTE *p64kBlock;
BYTE sectorCount;

BYTE *pBfrOrig;
BYTE *pBfr, *pBfrCnt;
BYTE *pDmaBuffer;

BYTE atariKeysToSingleByte(BYTE vkey, BYTE key);

BYTE loopForSetup(void);
BYTE loopForDownload(void);


BYTE gem_init(void);
void gem_deinit(void);
BYTE gem_floppySetup(void);

void uploadImage(int index);
void removeImage(int index);
void newImage(int index);
void downloadImage(int index);

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

        res = loopForDownload();    // show and handle floppy image download

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
	commandShort[0] = (deviceID << 5); 					            // cmd[0] = ACSI_id + TEST UNIT READY (0)

	graf_mouse(M_OFF, 0);

    while(1) {
        BYTE key;

        key = loopForSetup();

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

BYTE atariKeysToSingleByte(BYTE vkey, BYTE key)
{
	WORD vkeyKey;
	vkeyKey = (((WORD) vkey) << 8) | ((WORD) key);		/* create a WORD with vkey and key together */

    switch(vkeyKey) {
        case 0x5032: return KEY_PAGEDOWN;
        case 0x4838: return KEY_PAGEUP;
    }

	if(key >= 32 && key < 127) {		/* printable ASCII key? just return it */
		return key;
	}

	if(key == 0) {						/* will this be some non-ASCII key? convert it */
		switch(vkey) {
			case 0x48: return KEY_UP;
			case 0x50: return KEY_DOWN;
			case 0x4b: return KEY_LEFT;
			case 0x4d: return KEY_RIGHT;
			case 0x52: return KEY_INSERT;
			case 0x47: return KEY_HOME;
			case 0x62: return KEY_HELP;
			case 0x61: return KEY_UNDO;
			case 0x3b: return KEY_F1;
			case 0x3c: return KEY_F2;
			case 0x3d: return KEY_F3;
			case 0x3e: return KEY_F4;
			case 0x3f: return KEY_F5;
			case 0x40: return KEY_F6;
			case 0x41: return KEY_F7;
			case 0x42: return KEY_F8;
			case 0x43: return KEY_F9;
			case 0x44: return KEY_F10;
			default: return 0;			/* unknown key */
		}
	}

	switch(vkeyKey) {					/* some other no-ASCII key, but check with vkey too */
		case 0x011b: return KEY_ESC;
		case 0x537f: return KEY_DELETE;
		case 0x0e08: return KEY_BACKSP;
		case 0x0f09: return KEY_TAB;
		case 0x1c0d: return KEY_ENTER;
		case 0x720d: return KEY_ENTER;
	}

	return 0;							/* unknown key */
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

    int16_t res = rsrc_load("FDD.RSC");
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

BYTE getSelectedSlotNo(void)
{
    int32_t s1, s2, s3;
    rsrc_gaddr(R_OBJECT, RADIO_SLOT1, &s1);
    rsrc_gaddr(R_OBJECT, RADIO_SLOT2, &s2);
    rsrc_gaddr(R_OBJECT, RADIO_SLOT3, &s3);

    if(!s1 || !s2 || !s3) {
        return 0;
    }

    OBJECT *o1 = (OBJECT *) s1;
    OBJECT *o2 = (OBJECT *) s2;
    OBJECT *o3 = (OBJECT *) s3;

    if(o1->ob_state & OS_SELECTED) {
        return 1;
    }

    if(o2->ob_state & OS_SELECTED) {
        return 2;
    }

    if(o3->ob_state & OS_SELECTED) {
        return 3;
    }

    return 0;
}

BYTE gem_floppySetup(void)
{
    int32_t tree;
    rsrc_gaddr(R_TREE, FDD, &tree);                                             // get address of dialog tree

    int16_t xdial, ydial, wdial, hdial, exitobj;
    form_center((OBJECT *) tree, &xdial, &ydial, &wdial, &hdial);               // center object
    form_dial(0, 0, 0, 0, 0, xdial, ydial, wdial, hdial);                      // reserve screen space for dialog
    objc_draw((OBJECT *) tree, ROOT, MAX_DEPTH, xdial, ydial, wdial, hdial);    // draw object tree

    BYTE retVal = KEY_F10;

    while(1) {
        exitobj = form_do((OBJECT *) tree, 0) & 0x7FFF;

        if(exitobj == BTN_EXIT) {
            retVal = KEY_F10;   // KEY_F10 - quit
            break;
        }

        if(exitobj == BTN_INTERNET) {
            retVal = KEY_F9;   // KEY_F9 -- download images from internet
            break;
        }

        // TODO: unselect button

        BYTE slotNo = getSelectedSlotNo();

        if(!slotNo) {               // failed to get slot number? try once again
            continue;
        }

        if(exitobj == BTN_LOAD) {   // load image into slot
            uploadImage(slotNo - 1);
            continue;
        }

        if(exitobj == BTN_CLEAR) {  // remove image from slot
            removeImage(slotNo - 1);
            continue;
        }

        if(exitobj == BTN_NEW) {    // create new empty image in slot
            newImage(slotNo - 1);
            continue;
        }

        if(exitobj == BTN_SAVE) {   // save content of slot to file
            downloadImage(slotNo - 1);
            continue;
        }
    }

    form_dial (3, 0, 0, 0, 0, xdial, ydial, wdial, hdial);      // release screen space
    return retVal;
}
