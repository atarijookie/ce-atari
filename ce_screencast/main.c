/*--------------------------------------------------*/
//#include <tos.h>
#include <mint/osbind.h> 
#include <mint/falcon.h> 
#include <mt_gem.h>
#include <stdio.h>
//>#include <screen.h>
#include <string.h>
#include <stdlib.h>

#include "acsi.h"
#include "translated.h"
#include "gemdos.h"
#include "mutex.h"
#include "find_ce.h"
/*--------------------------------------------------*/

void showHomeScreen(void);
void sendKeyDown(BYTE key);
void refreshScreen(void);							
void setResolution(void);
void showConnectionErrorMessage(void);
BYTE findDevice(void);
void getDriveConfig(void);
extern void getConfig(void); 
BYTE checkForSupportedResolution(void);

BYTE atariKeysToSingleByte(BYTE vkey, BYTE key);
BYTE ce_identify(BYTE ACSI_id);
/*--------------------------------------------------*/
BYTE      deviceID;

BYTE commandShort[CMD_LENGTH_SHORT]	= {			0, 'C', 'E', HOSTMOD_TRANSLATED_DISK, 0, 0};
BYTE commandLong[CMD_LENGTH_LONG]	= {0x1f,	0, 'C', 'E', HOSTMOD_TRANSLATED_DISK, 0, 0, 0, 0, 0, 0, 0, 0}; 

#define DMA_BUFFER_SIZE		512*4

BYTE dmaBuffer[DMA_BUFFER_SIZE+2];
BYTE *pDmaBuffer; 

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

extern BYTE machine;

char *version = "2016-05-08"; 

volatile mutex mtx;   
/*--------------------------------------------------*/
int main(void)
{
	BYTE found; 	
	DWORD scancode;
	BYTE key, vkey, res;
	DWORD toEven;
	void *OldSP;

	//initialize lock
	mutex_unlock(&mtx);
 
	OldSP = (void *) Super((void *)0);  			/* supervisor mode */ 
	
	prevCommandFailed = 0;
	
	/* ---------------------- */
	/* create buffer pointer to even address */
	toEven = (DWORD) &dmaBuffer[0];
  
	if(toEven & 0x0001)       /* not even number? */
		toEven++;
  
	pDmaBuffer = (BYTE *) toEven; 

	Clear_home();

	(void) Cconws("\33p[ CosmosEx screencast driver ]\r\n[             ver "); 
    (void) Cconws(version);
    (void) Cconws(" ]\33q\r\n"); 		
	/* ---------------------- */
	/* search for device on the ACSI bus */
	found=findDevice();
	if( !found ){
    	(void) Cconws("Quit."); 		
	    Super((void *)OldSP);  			      /* user mode */
		return 0;
	}
	/* ----------------- */

	if( checkForSupportedResolution()==FALSE )
	{
		(void) Cconws("\r\n");
		(void) Cconws("Wrong resolution!\r\n");
		(void) Cconws("Only the following resolutions\r\n");
		(void) Cconws("are supported:\r\n");
		(void) Cconws("\r\n");
		(void) Cconws("320*200*16\r\n");
		(void) Cconws("640*200*4\r\n");
		(void) Cconws("640*200*2\r\n");
		(void) Cconws("\r\n");
		(void) Cconws("Screencast not installed.\r\n");
    	(void) Cconws("Press any key to quit T to try anyway.\r\n"); 		
		key=Cnecin();
		if( key!='t' ) 
		{
	    	Super((void *)OldSP);  			      /* user mode */
			return 0;
		}else{
			(void) Cconws("\r\n");
			(void) Cconws("\33pWARNING\33q: this will probably crash.\r\n");
		}
	}

	(void) Cconws("\r\n");
	(void) Cconws("Do not change resolution after this point!\r\n");
	(void) Cconws("\r\n");

	/* now set up the acsi command bytes so we don't have to deal with this one anymore */
	commandShort[0] = (deviceID << 5); 					/* cmd[0] = ACSI_id + TEST UNIT READY (0)	*/
	
	commandLong[0] = (deviceID << 5) | 0x1f;			/* cmd[0] = ACSI_id + ICD command marker (0x1f)	*/
	commandLong[1] = 0xA0;								/* cmd[1] = command length group (5 << 5) + TEST UNIT READY (0) */ 	

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

void getDriveConfig(void)
{
    getConfig();
 
    ceTranslatedDriveMap = pDmaBuffer[0]<<8|pDmaBuffer[1];

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

/*
	Check if we have a resolution, that resembles an ST compatible screenbuffer
	Problem: XBIOS lies on newer systems and with GFX cards, so try to determine
	an ST compatible mode by combination of screen buffer size (F030) and screen dimension/colors
*/
BYTE checkForSupportedResolution(void)
{
 	short vdi_work_in[11] = {1,1,1,1,1,1,1,1,1,1,2};
	short vdi_work_out[57];
	VdiHdl vdi_handle;
	int scr_x, scr_y, colors;
	BYTE result=FALSE;

   	v_opnvwk(vdi_work_in, &vdi_handle, vdi_work_out);
	v_clsvwk(vdi_handle);

    scr_x=vdi_work_out[0]+1;
	scr_y=vdi_work_out[1]+1;
	colors=vdi_work_out[13];

	switch(machine)
	{
		case MACHINE_FALCON:
			//check screen buffer size
			if( VgetSize( VsetMode(-1) )!=32000 )
			{
				return FALSE;
			}
		case MACHINE_ST:
		case MACHINE_TT:
			if( scr_x==320 && scr_y==200 && colors==16 )
			{
				return TRUE;				
			}   			
			if( scr_x==640 && scr_y==200 && colors==4 )
			{
				return TRUE;				
			}   			
			if( scr_x==640 && scr_y==400 && colors==2 )
			{
				return TRUE;				
			}   			
			break;
	}
	return FALSE;				
}