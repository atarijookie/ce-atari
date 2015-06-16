/*--------------------------------------------------*/
#include <mint/osbind.h> 
#include <mint/linea.h> 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "acsi.h"
#include "translated.h"
#include "gemdos.h"
#include "gemdos_errno.h"
#include "VT52.h"
#include "Cookiejar.h"

/*--------------------------------------------------*/

void showConnectionErrorMessage(void);
BYTE findDevice(void);
void getDriveConfig(void);
int getConfig(void); 
int readHansTest( int byteCount, WORD xorVal );
int writeHansTest( int byteCount, WORD xorVal );

BYTE ce_identify(BYTE ACSI_id);
/*--------------------------------------------------*/
BYTE      deviceID;

BYTE commandShort[CMD_LENGTH_SHORT]	= {			0, 'C', 'E', HOSTMOD_TRANSLATED_DISK, 0, 0};
BYTE commandLong[CMD_LENGTH_LONG]	= {0x1f,	0, 'C', 'E', HOSTMOD_TRANSLATED_DISK, 0, 0, 0, 0, 0, 0, 0, 0}; 

BYTE myBuffer[255*512+1];
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

/*--------------------------------------------------*/
int main(void)
{
	
	DWORD scancode;
	BYTE key, vkey, res;
	DWORD toEven;
	void *OldSP;
	WORD xorVal=0xC0DE;
	WORD testGlobalCount=0;
	int charcnt=0;
	int linecnt=0;
	int errcnt_crc_r=0,errcnt_crc_w=0;
	int errcnt_timeout_r=0,errcnt_timeout_w=0;
	int runcnt=0;

	OldSP = (void *) Super((void *)0);  			/* supervisor mode */
	lineaa();	/* hide mouse */   
	
	prevCommandFailed = 0;
	
	/* ---------------------- */
	/* create buffer pointer to even address */
	toEven = (DWORD) &myBuffer[0];
  
	if(toEven & 0x0001)       /* not even number? */
		toEven++;
  
	pBuffer = (BYTE *) toEven; 

	Clear_home();

	/* ---------------------- */
	print_head();
	(void) Cconws("\r\n");
	(void) Cconws(" Non-destructive ACSI read/write test.\r\n");
	(void) Cconws(" Helpful to detect possible DMA problems\r\n"); 		
	(void) Cconws(" your ST hardware might have. See:\r\n"); 		
	(void) Cconws(" http://joo.kie.sk/?page_id=250 and \r\n");
	(void) Cconws(" http://goo.gl/23AqXk for infos+fixes.\r\n"); 		
	
	unsigned long *cescreencast_cookie=0;
	if( CookieJarRead(0x43455343,&cescreencast_cookie)!=0 ) /* Cookie "CESC" */
	{ 
		(void) Cconws("\r\n");
		(void) Cconws(" CosmosEx Screencast is active. Please\r\n");
		(void) Cconws(" deactivate. \r\n\r\n Press any key to quit.\r\n");
		Cnecin();
		(void) Cconws("Quit."); 		
		
		linea9();   
		Super((void *)OldSP);  			      /* user mode */
		return 0;
	}
	
	(void) Cconws("\r\n Press any key to start.\r\n\r\n"); 		
	Cnecin();
	  
	print_status(0,0,0);
	VT52_Clear_down();

	/* ---------------------- */
	/* search for device on the ACSI bus */
	deviceID=findDevice();
	if( deviceID == (BYTE)-1 )
	{
    	(void) Cconws("Quit."); 		

      	linea9();   
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

    (void) Cconws("Testing (*=OK,C=Crc,_=Timeout):\r\n"); 		

   	//FIXME: check key the TOS way        
	BYTE* ikbd=0xfffffc02;     
  	while( *ikbd==0x39 ){
  	}

  	VT52_Goto_pos(0,24);
	Cconws("R:");
  	while(*ikbd!=0x39)
  	{
        int res=0;
  
      	if( linecnt&1 ){
    		res=writeHansTest(512*255,xorVal);
      	}else{
    		res=readHansTest(512*255,xorVal);
      	}
    	switch( res )
		{
			case -1:
				Cconws("_");
				if( linecnt&1 ){
					errcnt_timeout_w++;
				}else{
					errcnt_timeout_r++;
				}
				break;
			case -2:
				Cconws("C");
				if( linecnt&1 ){
					errcnt_crc_w++;
				}else{
					errcnt_crc_r++;
				}
				break;
			case 0:
				Cconws("*");
				break;
			default:
				Cconws(".");
				break;
	    }
		charcnt++;
		VT52_Save_pos();
		print_status(runcnt,errcnt_crc_r,errcnt_crc_w,errcnt_timeout_r,errcnt_timeout_w);
		print_head();
		VT52_Load_pos();
		
		if( charcnt>=40-2 ){
			VT52_Save_pos();
			VT52_Goto_pos(0,3);
			VT52_Del_line();
			VT52_Load_pos();
			VT52_Goto_pos(0,24);
			charcnt=0;
			linecnt++;
			if( linecnt&1 ){
			    Cconws("W:");
			    xorVal++;  /* change eorval after R/W run */
			}else{
				runcnt++;
				Cconws("R:");
			}
		}
	}
	
    linea9();										/* show mouse */   
    Super((void *)OldSP);  			      			/* user mode */

	return 0;
}

void print_head()
{
  	VT52_Goto_pos(0,0);

	(void) Cconws("\33p[ CosmosEx ACSI Test    ver "); 
    showAppVersion();
    (void) Cconws(" ]\33q\r\n"); 		
}

void print_status(int runcnt, int errcnt_crc_r, int errcnt_crc_w, int errcnt_timeout_r, int errcnt_timeout_w)
{
	int failcnt=0;
	failcnt=errcnt_crc_r+errcnt_crc_w+errcnt_timeout_r+errcnt_timeout_w;
	if(failcnt>9999){
		failcnt=9999;
	}
	if(runcnt>9999){
		runcnt=9999;
	}
	if(errcnt_crc_r>9999){
		errcnt_crc_r=9999;
	}
	if(errcnt_crc_w>9999){
		errcnt_crc_w=9999;
	}
	if(errcnt_timeout_r>9999){
		errcnt_timeout_r=9999;
	}
	if(errcnt_timeout_w>9999){
		errcnt_timeout_w=9999;
	}
	
	VT52_Goto_pos(0,1);
	(void) Cconws("\33p[ Run:");
	showInt(runcnt, 4);
	(void) Cconws(" C=Crc(r:");
	  showInt(errcnt_crc_r, 4);
	(void) Cconws(") _=T/O(r:");
	  showInt(errcnt_timeout_r, 4);
	(void) Cconws(") ]\33q\r\n");
	
	(void) Cconws("\33p[Fail:");
	  showInt(failcnt, 4);
	(void) Cconws("      (w:");
	  showInt(errcnt_crc_w, 4);
	(void) Cconws(")      (w:");
	  showInt(errcnt_timeout_w, 4);
	(void) Cconws(") ]\33q\r\n");
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

/*--------------------------------------------------*/

void getDriveConfig(void)
{
    getConfig();
 
    ceTranslatedDriveMap = pBuffer[0]<<8|pBuffer[1];

}

/*--------------------------------------------------*/

int getConfig(void)
{
    WORD res;
    
	commandShort[0] = (deviceID << 5); 					                        // cmd[0] = ACSI_id + TEST UNIT READY (0)
	commandShort[4] = GD_CUSTOM_getConfig;
  
	res = acsi_cmd(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pBuffer, 1);		// issue the command and check the result
    
    if(res != OK) {                                                             // failed to get config?
        return -1;
    }
	return 0;
} 

/*--------------------------------------------------*/

int readHansTest( int byteCount, WORD xorVal ){
    WORD res;
    
	commandLong[4+1] = TEST_READ;

  //size to read
	commandLong[5+1] = (byteCount>>16)&0xFF;
	commandLong[6+1] = (byteCount>>8)&0xFF;
	commandLong[7+1] = (byteCount)&0xFF;

  //Word to XOR with data on CE side
	commandLong[8+1] = (xorVal>>8)&0xFF;
	commandLong[9+1] = (xorVal)&0xFF;

	res = acsi_cmd(ACSI_READ, commandLong, CMD_LENGTH_LONG, pBuffer, (byteCount+511)>>9 );		// issue the command and check the result
    
    if(res != OK) {                                                             // ACSI ERROR?
        return -1;
    }
    
    int i;
    WORD counter = 0;
    WORD data = 0;
    for(i=0; i<byteCount; i += 2) {
        data = counter ^ xorVal;       // create word
        if( !(pBuffer[i]==(data>>8) && pBuffer[i+1]==(data&0xFF)) ){
          return -2;
        }  
        counter++;
    }

    if(byteCount & 1) {                                 // odd number of bytes? add last byte
        BYTE lastByte = (counter ^ xorVal) >> 8;
        if( pBuffer[byteCount-1]!=lastByte ){
          return -2;
        }  
    }
    
	return 0;
  
}

/*--------------------------------------------------*/

int writeHansTest( int byteCount, WORD xorVal ){
    BYTE res;
    
	commandLong[4+1] = TEST_WRITE;

  //size to read
	commandLong[5+1] = (byteCount>>16)&0xFF;
	commandLong[6+1] = (byteCount>>8)&0xFF;
	commandLong[7+1] = (byteCount)&0xFF;

  //Word to XOR with data on CE side
	commandLong[8+1] = (xorVal>>8)&0xFF;
	commandLong[9+1] = (xorVal)&0xFF;

    int i;
    WORD counter = 0;
    WORD data = 0;
    for(i=0; i<byteCount; i += 2) {
        data = counter ^ xorVal;       // create word
        pBuffer[i] = (data>>8);
        pBuffer[i+1] = (data&0xFF);
        counter++;
    }

    if(byteCount & 1) {                                 // odd number of bytes? add last byte
        BYTE lastByte = (counter ^ xorVal) >> 8;
        pBuffer[byteCount-1]=lastByte;
    }

	res = acsi_cmd(ACSI_WRITE, commandLong, CMD_LENGTH_LONG, pBuffer, (byteCount+511)>>9 );		// issue the command and check the result
    
    if(res == E_CRC) {                                                            
        return -2;
    }
    if(res != E_OK) {                                                             
        return -1;
    }
    
	return 0;
  
}

