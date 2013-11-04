/*--------------------------------------------------*/
#include <tos.h>
#include <stdio.h>
#include <screen.h>
#include <string.h>
#include <stdlib.h>

#include "acsi.h"
/*--------------------------------------------------*/
BYTE CE_ReadRunningFW(BYTE ACSI_id, BYTE *buffer);

void getScreenStream(BYTE homeScreen);			/* if homeScreen=1, gets homescreen; for 0 it gets current screen */
void sendKeyDown(BYTE vkey, BYTE key);
/*--------------------------------------------------*/
BYTE      deviceID;

BYTE myBuffer[520];
BYTE *pBuffer;
/*--------------------------------------------------*/
void main(void)
{
  DWORD scancode;
  BYTE key, vkey, res;
  BYTE i;
  WORD timeNow, timePrev;
  DWORD toEven;
 
  /* ---------------------- */
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
      
			res = CE_ReadRunningFW(i, pBuffer);      /* try to read FW name */
      
			if(res == 1) {                           	/* if found the US */
				deviceID = i;                     		/* store the ACSI ID of device */
				break;
			}
		}
  
		if(res == 1) {                             /* if found, break */
			break;
		}
      
		printf(" - not found.\nPress any key to retry or 'Q' to quit.\n");
		key = Cnecin();
    
		if(key == 'Q' || key=='q') {
			return;
		}
	}
  
	printf("\n\nCosmosEx ACSI ID: %d\nFirmWare: %s", (int) deviceID, (char *)pBuffer);
	/* ----------------- */
	getScreenStream(1);							/* get the home screen */
	
	/* use Ctrl + C to quit */
	timePrev = Tgettime();
	
	while(1) {
		res = Bconstat(2);						/* see if there's something waiting from keyboard */
		
		if(res == 0) {							/* nothing waiting from keyboard? */
			timeNow = Tgettime();
			
			if((timeNow - timePrev) > 0) {		/* check if time changed (2 seconds passed) */
				timePrev = timeNow;
				
				getScreenStream(0);				/* display a new stream (if something changed) */
			}
			
			continue;							/* try again */
		}
	
		scancode = Cnecin();					/* get char form keyboard, no echo on screen */

		vkey	= (scancode>>16)	& 0xff;
		key		=  scancode			& 0xff;

		sendKeyDown(vkey, key);					/* send this key to device */
		getScreenStream(0);						/* and display the screen */
	}
}
/*--------------------------------------------------*/
void sendKeyDown(BYTE vkey, BYTE key)
{
	/* todo: send a command with these two bytes to CosmosEx device */
	
}
/*--------------------------------------------------*/
void getScreenStream(BYTE homeScreen)			/* if homeScreen=1, gets homescreen; for 0 it gets current screen */
{
	/* todo: send command over ACSI and receive screen stream to pBuffer */
	
	
	/* now display the buffer */
	Cconws((char *) pBuffer);	
}
/*--------------------------------------------------*/
BYTE CE_ReadRunningFW(BYTE ACSI_id, BYTE *buffer)
{
  WORD res;
  BYTE cmd[] = {0x1f, 0x20, 'U', 'S', 'C', 'u', 'r', 'n', 't', 'F', 'W'};
  
  cmd[0] = 0x1f | (ACSI_id << 5);  
  memset(buffer, 0, 512);               /* clear the buffer */
  
  res = acsi_cmd(1, cmd, 11, buffer);         /* read name and version of current FW */
    
  if(res != OK)                         /* if failed, return FALSE */
    return 0;
    
  return 1;                             /* success */
}
/*--------------------------------------------------*/



