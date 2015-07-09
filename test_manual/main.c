//--------------------------------------------------
#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <support.h>

#include <stdint.h>
#include <stdio.h>

#include "stdlib.h"
#include "acsi.h"
#include "global.h"
#include "translated.h"
#include "gemdos.h"

//--------------------------------------------------

#define E_OK                0   	// 00 No error
#define E_CRC               0xfc	// -4  // fc CRC error

BYTE deviceID;                          // bus ID from 0 to 7

#define BUFFER_SIZE         (256*512 + 4)
BYTE myBuffer[BUFFER_SIZE];
BYTE *pBuffer;

void showHexByte (BYTE val);
void showHexWord (WORD val);
void showHexDword(DWORD val);

void TTresetscsi(void);

void cs_inquiry(BYTE id);
void CEread(void);
void findDevice(void);

void CEwrite(void);
int  writeHansTest(int byteCount, WORD xorVal);

BYTE showLogs = 1;
void showMenu(void);

//--------------------------------------------------

BYTE scsi_cmd_TT        (BYTE readNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount);
BYTE scsi_cmd_Falcon    (BYTE readNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount);

typedef BYTE (*THddIfCmd)(BYTE readNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount);

THddIfCmd hddIfCmd = NULL;
int ifUsed;

#define IF_ACSI         0
#define IF_SCSI_TT      1
#define IF_SCSI_FALCON  2

//--------------------------------------------------
int main(void)
{
	DWORD scancode;
	BYTE key;
	DWORD toEven;
	void *OldSP;

	OldSP = (void *) Super((void *)0);  			                // supervisor mode  

	// ---------------------- 
	// create buffer pointer to even address 
	toEven = (DWORD) &myBuffer[0];
  
	if(toEven & 0x0001)       // not even number? 
		toEven++;
  
	pBuffer = (BYTE *) toEven; 
	
	// ---------------------- 
	// search for device on the ACSI / SCSI bus 
	deviceID = 0;

	Clear_home();
    
    // choose the HDD interface
    (void) Cconws("Choose HDD interface:\r\n");
    
    while(1) {
        (void) Cconws("'A' - ACSI \r\n"); 		
        (void) Cconws("'T' - TT SCSI \r\n"); 		
        (void) Cconws("'F' - Falcon SCSI \r\n\r\n"); 

        key = Cnecin();
        if(key >= 'A' && key <= 'Z') {
            key += 32;
        }
        
        if(key == 't' || key == 'f' || key == 'a') {      // good key press? go on...
            break;
        }
    }

  	Clear_home();
    showMenu();

    switch(key) {
        case 'a':
            (void) Cconws("Using ACSI...\r\n"); 		
            hddIfCmd    = (THddIfCmd) acsi_cmd;
            ifUsed      = IF_ACSI;
            break;

        case 't':
            (void) Cconws("Using TT SCSI...\r\n"); 		
            hddIfCmd    = (THddIfCmd) scsi_cmd_TT;
            ifUsed      = IF_SCSI_TT;
            break;

        case 'f':
            (void) Cconws("Using Falcon SCSI...\r\n"); 		
            hddIfCmd    = (THddIfCmd) scsi_cmd_Falcon;
            ifUsed      = IF_SCSI_FALCON;
            break;
	} 

    while(1) {
        scancode = Bconin(DEV_CONSOLE); 		                    // get char form keyboard, no echo on screen 

        key		=  scancode & 0xff;
        
        if(key >= 'A' && key <= 'Z') {
            key += 32;
        }

        if(key == 'q') {
            break;
        }

        if(key == 'x') {
            TTresetscsi();
            continue;
        }
        
        if(key == 'i') {
            cs_inquiry(0);
            continue;
        }

        if(key == 'r') {            // read 
            CEread();
            continue;
        }

        if(key == 'w') {            // write
            CEwrite();
            continue;
        }

        if(key == 'f') {
            showLogs = 0;           // turn off logs - there will be errors on findDevice when device doesn't exist 
            Supexec(findDevice);
            showLogs = 1;           // turn on logs
            deviceID = 0;           // put device ID back to normal
            continue;
        }
        
        if(key == 'c') {
            Clear_home();
            showMenu();
            continue;
        }
    }
	
    Super((void *)OldSP);  			      			                // user mode 
	return 0;
}

void showMenu(void)
{
    (void) Cconws("X - SCSI RESET \r\n");
    (void) Cconws("I - SCSI Inquiry \r\n");
    (void) Cconws("R - CE READ test  \r\n");
    (void) Cconws("W - CE WRITE test \r\n");
    (void) Cconws("F - Find device on SCSI\r\n");
    (void) Cconws("C - clear screen\r\n");
    (void) Cconws("Q - quit\r\n");
}

BYTE commandLong[CMD_LENGTH_LONG] = {0x1f,	0, 'C', 'E', HOSTMOD_TRANSLATED_DISK, 0, 0, 0, 0, 0, 0, 0, 0}; 
int readHansTest( int byteCount, WORD xorVal);

void CEread(void)
{
  	commandLong[0] = (deviceID << 5) | 0x1f;			/* cmd[0] = ACSI_id + ICD command marker (0x1f)	*/
	commandLong[1] = 0xA0;								/* cmd[1] = command length group (5 << 5) + TEST UNIT READY (0) */ 	

    WORD xorVal=0xC0DE;
    
    int res = readHansTest(512*255, xorVal);
    
    switch( res )
    {
        case -1:    (void) Cconws("TIMEOUT\r\n");   break;
        case -2:    (void) Cconws("CRC FAIL\r\n");  break;
        case 0:     (void) Cconws("GOOD\r\n");      break;
        default:    (void) Cconws("ERROR\r\n");     break;
    }
}

int readHansTest(int byteCount, WORD xorVal)
{
    WORD res;
    
	commandLong[4+1] = TEST_READ;

  //size to read
	commandLong[5+1] = (byteCount >>16) & 0xFF;
	commandLong[6+1] = (byteCount >> 8) & 0xFF;
	commandLong[7+1] = (byteCount    )  & 0xFF;

  //Word to XOR with data on CE side
	commandLong[8+1] = (xorVal >> 8) & 0xFF;
	commandLong[9+1] = (xorVal     ) & 0xFF;

    (void) Cconws("CE READ: ");
    res = (*hddIfCmd) (ACSI_READ, commandLong, CMD_LENGTH_LONG, pBuffer, (byteCount+511)>>9 );		// issue the command and check the result
    
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

void cs_inquiry(BYTE id)
{
	int res, i;
    BYTE cmd[6];
    
    memset(cmd, 0, 6);
    cmd[0] = (id << 5) | (SCSI_CMD_INQUIRY & 0x1f);
    cmd[4] = 32;                                // count of bytes we want from inquiry command to be returned
    
    // issue the inquiry command and check the result 
    (void) Cconws("SCSI Inquiry: ");
    res = (*hddIfCmd) (1, cmd, 6, pBuffer, 1);
    
    showHexByte(res);
    (void) Cconws("\r\nINQUIRY HEXA: ");

    for(i=0; i<32; i++) {
        showHexByte(pBuffer[i]);
        (void) Cconout(' ');
    }
    (void) Cconws("\r\n");
    
    (void) Cconws("INQUIRY CHAR: ");
    for(i=0; i<32; i++) {
        if(pBuffer[i] >= 32) {
            Cconout(pBuffer[i]);
        } else {
            Cconout(' ');
        }

        Cconout(' ');
        Cconout(' ');
    }
    (void) Cconws("\r\n");
}
//--------------------------------------------------
void showHexByte(BYTE val)
{
    int hi, lo;
    char tmp[3];
    char table[16] = {"0123456789ABCDEF"};
    
    hi = (val >> 4) & 0x0f;;
    lo = (val     ) & 0x0f;

    tmp[0] = table[hi];
    tmp[1] = table[lo];
    tmp[2] = 0;
    
    (void) Cconws(tmp);
}

void showHexWord(WORD val)
{
    BYTE a,b;
    a = val >>  8;
    b = val;
    
    showHexByte(a);
    showHexByte(b);
}

void showHexDword(DWORD val)
{
    BYTE a,b,c,d;
    a = val >> 24;
    b = val >> 16;
    c = val >>  8;
    d = val;
    
    showHexByte(a);
    showHexByte(b);
    showHexByte(c);
    showHexByte(d);
}

BYTE ce_identify(BYTE bus_id)
{
  WORD res;
  BYTE cmd[] = {0, 'C', 'E', HOSTMOD_TRANSLATED_DISK, TRAN_CMD_IDENTIFY, 0};
  
  cmd[0] = (bus_id << 5); 					/* cmd[0] = ACSI_id + TEST UNIT READY (0)	*/
  memset(pBuffer, 0, 512);              	/* clear the buffer */

//  deviceID = bus_id;
  res = (*hddIfCmd)(1, cmd, 6, pBuffer, 1);	/* issue the identify command and check the result */
    
  if(res != OK)                         	/* if failed, return FALSE */
    return 0;
    
  if(strncmp((char *) pBuffer, "CosmosEx translated disk", 24) != 0) {		/* the identity string doesn't match? */
	 return 0;
  }
	
  return 1;                             /* success */
}

void findDevice(void)
{
	BYTE i;
	BYTE res;
	char bfr[2];

	bfr[1] = 0; 
	(void) Cconws("Looking for CosmosEx\n\r");

    for(i=0; i<8; i++) {
        (void) Cconws("[ SCSI ");

		bfr[0] = i + '0';
		(void) Cconws(bfr);
        (void) Cconws("] : ");
        		      
		res = ce_identify(i);      					/* try to read the IDENTITY string */
        if(res) {
            (void) Cconws("OK\n\r");
        } else {
            (void) Cconws("--\n\r");
        }
	}
}

void CEwrite(void)
{
  	commandLong[0] = (deviceID << 5) | 0x1f;			/* cmd[0] = ACSI_id + ICD command marker (0x1f)	*/
	commandLong[1] = 0xA0;								/* cmd[1] = command length group (5 << 5) + TEST UNIT READY (0) */ 	

    WORD xorVal=0xC0DE;
    
  	int res = writeHansTest(512*255, xorVal);
    
    switch( res )
    {
        case -1:    (void) Cconws("TIMEOUT\r\n");   break;
        case -2:    (void) Cconws("CRC FAIL\r\n");  break;
        case 0:     (void) Cconws("GOOD\r\n");      break;
        default:    (void) Cconws("ERROR\r\n");     break;
    }
}
        
int writeHansTest(int byteCount, WORD xorVal)
{
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

    (void) Cconws("CE WRITE: ");
    res = (*hddIfCmd) (ACSI_WRITE, commandLong, CMD_LENGTH_LONG, pBuffer, (byteCount+511)>>9 );		// issue the command and check the result
    
    if(res == E_CRC) {                                                            
        return -2;
    }
    if(res != E_OK) {                                                             
        return -1;
    }
    
	return 0;
}

void logMsg(char *logMsg)
{
    if(showLogs) {
        (void) Cconws(logMsg);
    }
}

void logMsgProgress(DWORD current, DWORD total)
{
    (void) Cconws("Progress: ");
    showHexDword(current);
    (void) Cconws(" out of ");
    showHexDword(total);
    (void) Cconws("\n\r");
}
