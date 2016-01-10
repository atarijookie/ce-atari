#include <mint/osbind.h> 
#include "acsi.h"
#include "stdlib.h"
#include "translated.h"

#include "screen.h"

extern BYTE deviceID;
extern BYTE *pDmaBuffer;
extern BYTE commandLong[CMD_LENGTH_LONG];

void writeScreen(BYTE command, BYTE screenmode, BYTE *bfr, DWORD cnt);
  
void sendScreenShot(void)
{
	WORD *pxPal         =   (WORD*)  0xffff8240;
	BYTE *pxScreen      = (BYTE *) (*((DWORD*) 0x44e));
	BYTE  screenMode    = (*((BYTE*)0xffff8260)) & 3;
    
    // send screen memory 
	writeScreen(TRAN_CMD_SENDSCREENCAST, screenMode, pxScreen, 32000);
	
    // send 16 ST palette entries 
	memcpy(pDmaBuffer, pxPal, 16*2);
	writeScreen(TRAN_CMD_SCREENCASTPALETTE, screenMode, pDmaBuffer, 16*2);
}  
   
void writeScreen(BYTE command, BYTE screenmode, BYTE *bfr, DWORD cnt)
{
	commandLong[5] = command; 
	commandLong[6] = screenmode;										// screenmode 
	
	commandLong[7] = cnt >> 16;											// store byte count 
	commandLong[8] = cnt >>  8;
	commandLong[9] = cnt  & 0xff;
	
	WORD sectorCount = cnt / 512;										// calculate how many sectors should we transfer 
	
	if((cnt % 512) != 0) {												// and if we have more than full sector(s) in buffer, send one more! 
		sectorCount++;
	}
	
	acsi_cmd(ACSI_WRITE, commandLong, CMD_LENGTH_LONG, bfr, sectorCount);	// send command to host over ACSI 
} 
