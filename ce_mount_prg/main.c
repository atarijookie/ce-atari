#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <unistd.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "translated.h"
#include "acsi.h"
#include "keys.h"
#include "find_ce.h"
#include "hdd_if.h"

#define DMA_BUFFER_SIZE		512

BYTE dmaBuffer[DMA_BUFFER_SIZE + 2];
BYTE *pDmaBuffer;

char driveLines[1024];

BYTE deviceID;

BYTE commandShort[CMD_LENGTH_SHORT]	= {	0, 'C', 'E', HOSTMOD_TRANSLATED_DISK, 0, 0};

void ce_getDrivesInfo(void);
void getDriveLine(int index, char *lines, char *line, int maxLen);

BYTE getKey(void);
BYTE atariKeysToSingleByte(BYTE vkey, BYTE key);

void showMounts(void);
void showMenu(void);
void unmount(BYTE drive);
void ce_unmount(void);

/* ------------------------------------------------------------------ */
int main( int argc, char* argv[] )
{
	BYTE found;

	/* create buffer pointer to even address */
	pDmaBuffer = &dmaBuffer[2];
	pDmaBuffer = (BYTE *) (((DWORD) pDmaBuffer) & 0xfffffffe);		/* remove odd bit if the address was odd */

	// search for CosmosEx on ACSI & SCSI bus
	found = Supexec(findDevice);

	if(!found) {								            // not found? quit
		sleep(3);
		return 0;
	} 
    
	/* now set up the acsi command bytes so we don't have to deal with this one anymore */
	commandShort[0] = (deviceID << 5); 					/* cmd[0] = ACSI_id + TEST UNIT READY (0)	*/
	
    showMenu();
    
    while(1) {
        BYTE key = getKey();
    
        if(key == KEY_F10) {
            break;
        }
        
        if(key >= 'a' && key <= 'z') {      // to lower case
            key = key - 32;
        }
        
        if(key >= 'C' && key <= 'O') {
            unmount(key);
        }
        
        showMenu();
    }
	
	return 0;		
}

void unmount(BYTE drive)
{
    if(drive < 'C' || drive > 'O') {
        return;
    }
    
    commandShort[5] = drive - 'A';          // contains drive index to unmount
    Supexec(ce_unmount);
}

void showMenu(void)
{
    (void) Clear_home();
    (void) Cconws("\33p[CosmosEx mount tool, by Jookie 2014]\33q\r\n");
    (void) Cconws("Press from C to P to unmount drive.\r\n");
    (void) Cconws("Press F10 to quit.\r\n\r\n");

    showMounts();
}

void showMounts(void)
{
	int row;
	char driveLine[128];
	
	Supexec(ce_getDrivesInfo);
	
	for(row=0; row<14; row++) {
		getDriveLine(row, driveLines, driveLine, 128);		
		(void) Cconws(driveLine);
		(void) Cconws("\r\n");
    }
}

void getDriveLine(int index, char *lines, char *line, int maxLen)
{
	int len, cnt=0, i;
	
	len = strlen(lines);

	for(i=0; i<len; i++) {								// find starting position
		if(cnt == index) {								// if got the right count on '\n', break
			break;
		}
	
		if(lines[i] == '\n') {							// found '\n'? Update counter
			cnt++;
		}
	}
	
	if(i == len) {										// index not found?
		line[0] = 0;
		return;
	}
	
	int j;
	for(j=0; j<maxLen; j++) {
		if(lines[i] == 0 || lines[i] == '\n') {
			break;
		}
	
		line[j] = lines[i];
		i++;
	}
	line[j] = 0;										// terminate the string
}

void ce_unmount(void)
{
	commandShort[4] = ACC_UNMOUNT_DRIVE;
//	commandShort[5] = 0;            // fill this before calling this function
	
	// ask for new data
	(*hdIf.cmd)(ACSI_READ, commandShort, 6, pDmaBuffer, 1);
	
	// if failed to get data
	if(!hdIf.success || hdIf.statusByte != OK) {
        (void) Clear_home();
		(void) Cconws("CE connection fail...\n");
        getKey();
	}
}

void ce_getDrivesInfo(void)
{
	commandShort[4] = ACC_GET_MOUNTS;
	commandShort[5] = 0;
	
	// ask for new data
	(*hdIf.cmd)(ACSI_READ, commandShort, 6, pDmaBuffer, 1);
	
	// if failed to get data
	if(!hdIf.success || hdIf.statusByte != OK) {
		strcpy(driveLines, "CE connection fail\n");
		return;
	}
	
	// got data, copy them to the right buffer
	strcpy(driveLines, (char *) pDmaBuffer);
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
