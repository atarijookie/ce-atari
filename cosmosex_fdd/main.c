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
       
// ------------------------------------------------------------------ 
BYTE ce_findId(void);
BYTE ce_identify(void);

BYTE deviceID;
BYTE commandShort[CMD_LENGTH_SHORT]	= {	0, 'C', 'E', HOSTMOD_FDD_SETUP, 0, 0};

BYTE getLowestDrive(void);
void removeLastPartUntilBackslash(char *str);

void showComError(void);
void createFullPath(char *fullPath, char *filePath, char *fileName);

char filePath[256], fileName[256];

BYTE *p64kBlock;
BYTE sectorCount;

BYTE *pBfrOrig;
BYTE *pBfr, *pBfrCnt;

BYTE atariKeysToSingleByte(BYTE vkey, BYTE key);

BYTE loopForSetup(void);
BYTE loopForDownload(void);

// ------------------------------------------------------------------ 
int main( int argc, char* argv[] )
{
	BYTE found;
  
    appl_init();										            // init AES 
    
	pBfrOrig = (BYTE *) Malloc(SIZE64K + 4);
	
	if(pBfrOrig == NULL) {
		(void) Cconws("\r\nMalloc failed!\r\n");
		sleep(3);
		return 0;
	}

	DWORD val = (DWORD) pBfrOrig;
	pBfr      = (BYTE *) ((val + 4) & 0xfffffffe);     				// create even pointer
    pBfrCnt   = pBfr - 2;											// this is previous pointer - size of WORD 
	
    // init fileselector path
    strcpy(filePath, "C:\\*.*");                            
    memset(fileName, 0, 256);          
    
    BYTE drive = getLowestDrive();                                  // get the lowest HDD letter and use it in the file selector
    filePath[0] = drive;
    
	// write some header out
	(void) Clear_home();
	(void) Cconws("\33p[ CosmosEx floppy setup ]\r\n[    by Jookie 2014     ]\33q\r\n\r\n");

	// search for CosmosEx on ACSI bus
    
	found = ce_findId();
	if(!found) {								                    // not found? quit
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

			return 1;
		}
	}

	// if not found 
    (void) Cconws("\r\nCosmosEx not found on ACSI bus, not installing driver.");
	return 0;
}

// send an IDENTIFY command to specified ACSI ID and check if the result is as expected 
BYTE ce_identify(void)
{
	WORD res;
  
	commandShort[0] = (deviceID << 5); 											// cmd[0] = ACSI_id + TEST UNIT READY (0)	
	commandShort[4] = FDD_CMD_IDENTIFY;
  
	memset(pBfr, 0, 512);              											// clear the buffer 

	res = acsi_cmd(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pBfr, 1);			// issue the command and check the result 

	if(res != OK) {                        										// if failed, return FALSE 
		return 0;
	}

	if(strncmp((char *) pBfr, "CosmosEx floppy setup", 21) != 0) {				// the identity string doesn't match? 
		return 0;
	}
	
	return 1;                             										// success 
}

// make single ACSI read command by the params set in the commandShort buffer
BYTE ce_acsiReadCommand(void)
{
	WORD res;
  
	commandShort[0] = (deviceID << 5); 											// cmd[0] = ACSI_id + TEST UNIT READY (0)	
  
	memset(pBfr, 0, 512);              											// clear the buffer 

	res = acsi_cmd(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pBfr, sectorCount);   // issue the command and check the result 

	return res;                            										// success 
}

BYTE ce_acsiWriteBlockCommand(void)
{
	WORD res;
  
	commandShort[0] = (deviceID << 5); 											// cmd[0] = ACSI_id + TEST UNIT READY (0)	
  
	res = acsi_cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, p64kBlock, sectorCount);	// issue the command and check the result 

    return res;                                                                 // just return the code
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
