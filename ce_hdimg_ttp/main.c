// vim: expandtab shiftwidth=4 tabstop=4
#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>

#include <stdint.h>
#include <stdio.h>

#include "../ce_hdd_if/stdlib.h"
#include "../ce_hdd_if/hdd_if.h"
#include "main.h"
#include "hostmoddefs.h"
#include "keys.h"
#include "defs.h"
       
BYTE getKey(void);
BYTE atariKeysToSingleByte(BYTE vkey, BYTE key);

// ------------------------------------------------------------------ 

BYTE deviceID;
BYTE commandShort[CMD_LENGTH_SHORT]	= {	0, 'C', 'E', HOSTMOD_CONFIG, CFG_CMD_SET_CFGVALUE, 0};

WORD buffer[512/2];

void showComError(void);


// ------------------------------------------------------------------ 
int main( int argc, char* argv[] )
{
    int i;

	// write some header out
	(void) Clear_home();
	(void) Cconws("\33p[  CosmosEx HDD IMAGE TTP  ]\r\n[    by nanard 2016     ]\33q\r\n\r\n");
    
    char *params        = (char *) argv;                            // get pointer to params (path to file)
    int paramsLength    = (int) params[0];
    char *path          = params + 1;
   
    if(paramsLength == 0) {
        (void) Cconws("This is a drap-and-drop, tool\r\n"
                      "to set HDD Image.\r\n\r\n"
                      "\33pArgument is path to HDD image.\33q\r\n\r\n");
        getKey();
        return 0;
    }

    path[paramsLength]  = 0;                                        // terminate path
    (void) Cconws(path);
    (void) Cconws("\r\n");
    
    // search for CosmosEx on ACSI & SCSI bus
    deviceID = Supexec(findDevice);

    if(deviceID == DEVICE_NOT_FOUND) {
        sleep(3);
        return 0;
    }

	// now set up the acsi command bytes so we don't have to deal with this one anymore 
	commandShort[0] = (deviceID << 5); 					            // cmd[0] = ACSI_id + TEST UNIT READY (0)	

    char * cbuffer = (char *)buffer;
    memcpy(buffer, "CECFG1", 6);
    i = 6;
    cbuffer[i++] = CFGVALUE_TYPE_ST_PATH;
    memcpy(cbuffer+i, "HDDIMAGE", 9);
    i += 9;
    cbuffer[i++] = strlen(path) + 1;
    memcpy(cbuffer + i, path, strlen(path) + 1);

    BYTE ret = Supexec(ce_acsiWriteBlockCommand);

    if(ret != 0) {
        (void)Cconws("\r\n***FAILED***\r\n");
    } else {
        (void)Cconws("\r\nOK. \33pPlease RESET\33q\r\n");
    }
    getKey();
	return 0;		
}

#if 0
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
#endif

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

BYTE ce_acsiWriteBlockCommand(void)
{
	commandShort[0] = (deviceID << 5); 											// cmd[0] = ACSI_id + TEST UNIT READY (0)	
  
	(*hdIf.cmd)(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, (BYTE *)buffer, 1);	// issue the command and check the result 

    if(!hdIf.success) {
        return 0xff;
    }
    
	return hdIf.statusByte;
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

void logMsg(char *logMsg)
{
}

void logMsgProgress(DWORD current, DWORD total)
{
}
