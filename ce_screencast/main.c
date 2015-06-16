/*--------------------------------------------------*/
//#include <tos.h>
#include <mint/osbind.h> 
#include <stdio.h>
//>#include <screen.h>
#include <string.h>
#include <stdlib.h>

#include "acsi.h"
#include "translated.h"
#include "gemdos.h"
/*--------------------------------------------------*/

void showHomeScreen(void);
void sendKeyDown(BYTE key);
void refreshScreen(void);							
void setResolution(void);
void showConnectionErrorMessage(void);
BYTE findDevice(void);
void getDriveConfig(void);
extern void getConfig(void); 

BYTE atariKeysToSingleByte(BYTE vkey, BYTE key);
BYTE ce_identify(BYTE ACSI_id);
/*--------------------------------------------------*/
BYTE      deviceID;

BYTE commandShort[CMD_LENGTH_SHORT]	= {			0, 'C', 'E', HOSTMOD_TRANSLATED_DISK, 0, 0};
BYTE commandLong[CMD_LENGTH_LONG]	= {0x1f,	0, 'C', 'E', HOSTMOD_TRANSLATED_DISK, 0, 0, 0, 0, 0, 0, 0, 0}; 

BYTE myBuffer[4*512];
BYTE *pBuffer;

BYTE prevCommandFailed;

#define HOSTMOD_CONFIG				1
#define HOSTMOD_LINUX_TERMINAL		2
#define HOSTMOD_TRANSLATED_DISK		3
#define HOSTMOD_NETWORK_ADAPTER		4
#define HOSTMOD_FDD_SETUP           5

#define TRAN_CMD_IDENTIFY           0
#define TRAN_CMD_GETDATETIME        1

#define DATE_OK                              0
#define DATE_ERROR                           2
#define DATE_DATETIME_UNKNOWN                4

#define Clear_home()    Cconws("\33E")

WORD ceTranslatedDriveMap;

extern WORD _installed;
extern WORD _vblskipscreen;
extern WORD _vblskipconfig;

char *version = "2014-08-17"; 

/*--------------------------------------------------*/
int main(void)
{
	
	DWORD scancode;
	BYTE key, vkey, res;
	DWORD toEven;
	void *OldSP;

	OldSP = (void *) Super((void *)0);  			/* supervisor mode */ 
	
	prevCommandFailed = 0;
	
	/* ---------------------- */
	/* create buffer pointer to even address */
	toEven = (DWORD) &myBuffer[0];
  
	if(toEven & 0x0001)       /* not even number? */
		toEven++;
  
	pBuffer = (BYTE *) toEven; 

	Clear_home();

	(void) Cconws("\33p[ CosmosEx screencast driver ]\r\n[             ver "); 
    (void) Cconws(version);
    (void) Cconws(" ]\33q\r\n"); 		
	/* ---------------------- */
	/* search for device on the ACSI bus */
	deviceID=findDevice();
	if( deviceID == (BYTE)-1 ){
    	(void) Cconws("Quit."); 		
	    Super((void *)OldSP);  			      /* user mode */
		return 0;
	}
	/* ----------------- */

	/* now set up the acsi command bytes so we don't have to deal with this one anymore */
	commandShort[0] = (deviceID << 5); 					/* cmd[0] = ACSI_id + TEST UNIT READY (0)	*/
	
	commandLong[0] = (deviceID << 5) | 0x1f;			/* cmd[0] = ACSI_id + ICD command marker (0x1f)	*/
	commandLong[1] = 0xA0;								/* cmd[1] = command length group (5 << 5) + TEST UNIT READY (0) */ 	

    WORD currentDrive		= Dgetdrv();						/* get the current drive from system */ 
    getDriveConfig();                                     /* get translated disk configuration */ 
   
    
    if(ceTranslatedDriveMap & (1 << currentDrive))      /* did we start from translated drive? */ 
    {
        /* abort, ptermres isn't working from translated drives */				
        Cconws("Could not start driver from translated\r\n"); 
        Cconws("drive.\r\n");
        Cconws("Please copy this driver to e.g. drive C:\r\n");
        Cconws("and start it from there.\r\n");
        Cconws("TSRs can only be reliably loaded from\r\n");
        Cconws("SD card or raw drives.\r\n");
        Cconws("\33pAborted.\33q\r\n");
        Cconin();
    	return 0;
	}
        
	//only transfer screen every 10th VBL
	_vblskipscreen=10;
	//transfer config every 50th frame	
	_vblskipconfig=50;	
	init_screencapture();
	
    Cconin();
	
    Super((void *)OldSP);  			      			/* user mode */

	if( _installed!=0 ){
		Ptermres( 0x1000 + 0x100 + _base->p_tlen + _base->p_dlen + _base->p_blen, 0 ); 
	}

	return 0;
}
/*--------------------------------------------------*/
BYTE ce_identify(BYTE ACSI_id)
{
  WORD res;
  BYTE cmd[] = {0, 'C', 'E', HOSTMOD_TRANSLATED_DISK, TRAN_CMD_IDENTIFY, 0};
  
  cmd[0] = (ACSI_id << 5); 					/* cmd[0] = ACSI_id + TEST UNIT READY (0)	*/
  memset(pBuffer, 0, 512);              	/* clear the buffer */

  res = acsi_cmd(1, cmd, 6, pBuffer, 1);	/* issue the identify command and check the result */
    
  if(res != OK)                         	/* if failed, return FALSE */
    return 0;
    
  if(strncmp((char *) pBuffer, "CosmosEx translated disk", 24) != 0) {		/* the identity string doesn't match? */
	 return 0;
  }
	
  return 1;                             /* success */
}
/*--------------------------------------------------*/
void showConnectionErrorMessage(void)
{
//	Clear_home();
	Cconws("Communication with CosmosEx failed.\nWill try to reconnect in a while.\n\nTo quit to desktop, press F10\n");
	
	prevCommandFailed = 1;
}
/*--------------------------------------------------*/
BYTE findDevice()
{
	BYTE i;
	BYTE key, vkey, res;
	BYTE deviceID = 0;
	char bfr[2];

	bfr[1] = 0; 
	Cconws("Looking for CosmosEx: ");

	while(1) {
		for(i=0; i<8; i++) {
			bfr[0] = i + '0';
			(void) Cconws(bfr); 
		      
			res = ce_identify(i);      					/* try to read the IDENTITY string */
      
			if(res == 1) {                           	/* if found the CosmosEx */
				deviceID = i;                     		/* store the ACSI ID of device */
				break;
			}
		}
  
		if(res == 1) {                             		/* if found, break */
			break;
		}
      
		Cconws(" - not found.\r\nPress any key to retry or 'Q' to quit.\r\n");
		key = Cnecin();
    
		if(key == 'Q' || key=='q') {
			return -1;
		}
	}
  
	bfr[0] = deviceID + '0';
	Cconws("\r\nCosmosEx ACSI ID: ");
	Cconws(bfr);
	Cconws("\r\n\r\n");
	return deviceID;
}

void getDriveConfig(void)
{
    getConfig();
 
    ceTranslatedDriveMap = pBuffer[0]<<8|pBuffer[1];

}