#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <unistd.h>
#include <gem.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "acsi.h"
#include "main.h"
#include "hostmoddefs.h"
#include "keys.h"
#include "defs.h"
#include "hdd_if.h"
#include "find_ce.h"
       
// ------------------------------------------------------------------ 

BYTE deviceID;
BYTE commandShort[CMD_LENGTH_SHORT]	= {	0, 'C', 'E', HOSTMOD_FDD_SETUP, 0, 0};

void showComError(void);

BYTE *p64kBlock;
BYTE sectorCount;

BYTE *pBfrOrig;
BYTE *pBfr, *pBfrCnt;
BYTE *pDmaBuffer;

BYTE atariKeysToSingleByte(BYTE vkey, BYTE key);

BYTE getCurrentSlot(void);
BYTE setCurrentSlot(BYTE newSlot);
BYTE currentSlot;

BYTE uploadImage(int index, char *path);

BYTE getIsImageBeingEncoded(void);
#define ENCODING_DONE       0
#define ENCODING_RUNNING    1
#define ENCODING_FAIL       2

// ------------------------------------------------------------------ 
int main( int argc, char* argv[] )
{
	BYTE found, setSlot;

	// write some header out
	(void) Clear_home();
	(void) Cconws("\33p[  CosmosEx floppy TTP  ]\r\n[    by Jookie 2014     ]\33q\r\n\r\n");
    
    char *params        = (char *) argv;                            // get pointer to params (path to file)
    int paramsLength    = (int) params[0];
    char *path          = params + 1;
   
    if(paramsLength == 0) {
        (void) Cconws("This is a drap-and-drop, upload\r\n");
        (void) Cconws("and run floppy tool.\r\n\r\n");
        (void) Cconws("\33pArgument is path to floppy image.\33q\r\n\r\n");
        (void) Cconws("For menu driven floppy config\r\n");
        (void) Cconws("run the CE_FDD.PRG\r\n");
        getKey();
        return 0;
    }

    path[paramsLength]  = 0;                                        // terminate path
    
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
    
	// search for CosmosEx on ACSI bus
	found = Supexec(findDevice);

	if(!found) {								            // not found? quit
		sleep(3);
		return 0;
	}
 
	// now set up the acsi command bytes so we don't have to deal with this one anymore 
	commandShort[0] = (deviceID << 5); 					            // cmd[0] = ACSI_id + TEST UNIT READY (0)	

    BYTE res = getCurrentSlot();                                    // get the current slot
    
    if(!res) {
        Mfree(pBfrOrig);
        return 0;
    }

    setSlot = 0;
    if(currentSlot > 2) {                                           // current slot is out of index? (probably empty slot selected) Upload to slot #0
        currentSlot = 0;
        setSlot     = 1;                                            // we need to update the slot after upload and encode
    }
    
    // upload the image to CosmosEx
    res = uploadImage((int) currentSlot, path);                     // now try to upload
    
    if(!res) {
        (void) Cconws("Image upload failed, press key to terminate.\r\n");
        getKey();
        Mfree(pBfrOrig);
        return 0;
    }

    // wait until image is being encoded
    (void) Cconws("Please wait, processing image: ");
    
    while(1) {
        res = getIsImageBeingEncoded();
    
        if(res == ENCODING_DONE) {                                  // encoding went OK
            if(setSlot) {                                           // if we uploaded to slot #0 when empty slot was selected, switch to slot #0
                setCurrentSlot(currentSlot);
            }
        
            (void) Cconws("\n\rDone. Reset ST to boot from the uploaded floppy.\r\n");
            getKey();
            break;
        }

        if(res == ENCODING_FAIL) {                                  // encoding failed
            (void) Cconws("\n\rFinal phase failed, press key to terminate.\r\n");
            getKey();
            break;
        }
        
        sleep(1);                                                   // encoding still running
        (void) Cconws("*");
    }
    
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

    scancode = Cnecin();					/* get char form keyboard, no echo on screen */

	vkey	= (scancode >> 16)  & 0xff;
    key		=  scancode         & 0xff;

    key		= atariKeysToSingleByte(vkey, key);	/* transform BYTE pair into single BYTE */
    
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

void showComError(void)
{
    (void) Clear_home();
    (void) Cconws("Error in CosmosEx communication!\r\n");
    Cnecin();
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

BYTE getCurrentSlot(void)
{
    commandShort[4] = FDD_CMD_GET_CURRENT_SLOT;

    sectorCount = 1;                            // read 1 sector

    BYTE res = Supexec(ce_acsiReadCommand); 
		
	if(res == FDD_OK) {                         // good? copy in the results
        currentSlot = pBfr[0];
        return 1;
    } 
    
    // bad? show error
    showComError();
    return 0;
}

BYTE setCurrentSlot(BYTE newSlot)
{
    commandShort[4] = FDD_CMD_SET_CURRENT_SLOT;
    commandShort[5] = newSlot;
    
    sectorCount = 1;                            // read 1 sector

    BYTE res = Supexec(ce_acsiReadCommand); 
		
	if(res == FDD_OK) {                         // good? copy in the results
        return 1;
    } 
    
    // bad? show error
    showComError();
    return 0;
}

BYTE getIsImageBeingEncoded(void)
{
    commandShort[4] = FDD_CMD_GET_IMAGE_ENCODING_RUNNING;
    commandShort[5] = currentSlot;

    sectorCount = 1;                            // read 1 sector

    BYTE res = Supexec(ce_acsiReadCommand); 
		
	if(res == FDD_OK) {                         // good? copy in the results
        BYTE isRunning = pBfr[0];               // isRunning - 1: is running, 0: is not running
        return isRunning;
    } 
    
    // fail?
    showComError();                             // show error
    return ENCODING_FAIL;
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

