/*--------------------------------------------------*/
#include <tos.h>
#include <stdio.h>
#include <screen.h>
#include <string.h>
#include <stdlib.h>

#include "acsi.h"
#include "keys.h"
/*--------------------------------------------------*/

void showHomeScreen(void);
void sendKeyDown(BYTE key);
BYTE atariKeysToSingleByte(BYTE vkey, BYTE key);
BYTE ce_identify(BYTE ACSI_id);
/*--------------------------------------------------*/
BYTE      deviceID;

BYTE myBuffer[520];
BYTE *pBuffer;

#define HOSTMOD_CONFIG				1
#define HOSTMOD_LINUX_TERMINAL		2
#define HOSTMOD_TRANSLATED_DISK		3
#define HOSTMOD_NETWORK_ADAPTER		4

#define CFG_CMD_IDENTIFY			0
#define CFG_CMD_KEYDOWN				1
#define CFG_CMD_GO_HOME				0xff
/*--------------------------------------------------*/
int main(void)
{
	DWORD scancode;
	BYTE key, vkey, res;
	BYTE i;
	WORD timeNow, timePrev;
	DWORD toEven;
	void *OldSP;

	OldSP = (void *) Super((void *)0);  			/* supervisor mode */ 
	
	/* ---------------------- */
	/* create buffer pointer to even address */
	toEven = (DWORD) &myBuffer[0];
  
	if(toEven & 0x0001)       /* not even number? */
		toEven++;
  
	pBuffer = (BYTE *) toEven; 
	
	/* ---------------------- */
	/* search for device on the ACSI bus */
	deviceID = 0;

	Clear_home();
	printf("Looking for CosmosEx:\n");

	while(1) {
		for(i=0; i<8; i++) {
			printf("%d", i);
      
			res = ce_identify(i);      					/* try to read the IDENTITY string */
      
			if(res == 1) {                           	/* if found the CosmosEx */
				deviceID = i;                     		/* store the ACSI ID of device */
				break;
			}
		}
  
		if(res == 1) {                             		/* if found, break */
			break;
		}
      
		printf(" - not found.\nPress any key to retry or 'Q' to quit.\n");
		key = Cnecin();
    
		if(key == 'Q' || key=='q') {
		    Super((void *)OldSP);  			      /* user mode */
			return 0;
		}
	}
  
	printf("\n\nCosmosEx ACSI ID: %d\n", (int) deviceID);
	/* ----------------- */
	showHomeScreen();							/* get the home screen */
	
	/* use Ctrl + C to quit */
	timePrev = Tgettime();
	
	while(1) {
		res = Bconstat(2);						/* see if there's something waiting from keyboard */
		
		if(res == 0) {							/* nothing waiting from keyboard? */
			timeNow = Tgettime();
			
			if((timeNow - timePrev) > 0) {		/* check if time changed (2 seconds passed) */
				timePrev = timeNow;
				
				sendKeyDown(0);					/* display a new stream (if something changed) */
			}
			
			continue;							/* try again */
		}
	
		scancode = Cnecin();					/* get char form keyboard, no echo on screen */

		vkey	= (scancode>>16)	& 0xff;
		key		=  scancode			& 0xff;

		key		= atariKeysToSingleByte(vkey, key);	/* transform BYTE pair into single BYTE */
		
		if(key == KEY_F10) {						/* should quit? */
			break;
		}
		
		sendKeyDown(key);							/* send this key to device */
	}
	
    Super((void *)OldSP);  			      			/* user mode */
	return 0;
}
/*--------------------------------------------------*/
void sendKeyDown(BYTE key)
{
	WORD res;
	BYTE cmd[] = {0, 'C', 'E', HOSTMOD_CONFIG, CFG_CMD_KEYDOWN, 0};
	
	cmd[0] = (deviceID << 5); 						/* cmd[0] = ACSI_id + TEST UNIT READY (0)	*/
	cmd[5] = key;									/* store the pressed key to cmd[5] */
  
	memset(pBuffer, 0, 512);               			/* clear the buffer */
  
	res = acsi_cmd(1, cmd, 6, pBuffer, 1); 			/* issue the KEYDOWN command and show the screen stream */
    
	if(res != OK) {									/* if failed, return FALSE */
		printf("sendKeyDown failed!\n");
		return;
	}
	
	Cconws((char *) pBuffer);						/* now display the buffer */
}
/*--------------------------------------------------*/
void showHomeScreen(void)							
{
	WORD res;
	BYTE cmd[] = {0, 'C', 'E', HOSTMOD_CONFIG, CFG_CMD_GO_HOME, 0};
	
	cmd[0] = (deviceID << 5); 						/* cmd[0] = ACSI_id + TEST UNIT READY (0)	*/
	memset(pBuffer, 0, 512);               			/* clear the buffer */
  
	res = acsi_cmd(1, cmd, 6, pBuffer, 1); 			/* issue the GO_HOME command and show the screen stream */
    
	if(res != OK) {									/* if failed, return FALSE */
		printf("showHomeScreen failed!\n");
		return;
	}
	
	Cconws((char *) pBuffer);						/* now display the buffer */
}
/*--------------------------------------------------*/
BYTE ce_identify(BYTE ACSI_id)
{
  WORD res;
  BYTE cmd[] = {0, 'C', 'E', HOSTMOD_CONFIG, CFG_CMD_IDENTIFY, 0};
  
  cmd[0] = (ACSI_id << 5); 					/* cmd[0] = ACSI_id + TEST UNIT READY (0)	*/
  memset(pBuffer, 0, 512);              	/* clear the buffer */
  
  res = acsi_cmd(1, cmd, 6, pBuffer, 1);	/* issue the identify command and check the result */
    
  if(res != OK)                         	/* if failed, return FALSE */
    return 0;
    
  if(strncmp((char *) pBuffer, "CosmosEx config console", 23) != 0) {		/* the identity string doesn't match? */
	return 0;
  }
	
  return 1;                             /* success */
}
/*--------------------------------------------------*/
BYTE atariKeysToSingleByte(BYTE vkey, BYTE key)
{
	WORD vkeyKey;

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
	
	vkeyKey = (((WORD) vkey) << 8) | ((WORD) key);		/* create a WORD with vkey and key together */
	
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
/*--------------------------------------------------*/



