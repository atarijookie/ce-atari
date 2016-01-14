#include <mint/osbind.h> 
#include "hdd_if.h"
#include "stdlib.h"
#include "translated.h"

#include "main.h"
#include "screen.h"

extern BYTE deviceID;
extern BYTE *pDmaBuffer;
extern BYTE commandShort[CMD_LENGTH_SHORT];
extern BYTE commandLong[CMD_LENGTH_LONG];

extern volatile ScreenShots screenShots;    // screenshots config

void writeScreen(BYTE command, BYTE screenmode, BYTE *bfr, DWORD cnt);
  
void screenworker(void)
{
    // screenshots VBL not enabled? quit
    if(!screenShots.enabled) {              
        return;
    }

    //-------------
    // first update the screenshots config 
    commandShort[4] = TRAN_CMD_SCREENSHOT_CONFIG;
    commandShort[5] = 0;
    
    hdIf.forceFlock = 1;                    // let HD IF force FLOCK, as this FLOCK has been acquired in the asm code before this function
    (*hdIf.cmd)(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);  
    
    if(!hdIf.success) {                     // error? 
        hdIf.forceFlock = 0;                // force FLOCK back to normal
        return;
    }

    screenShots.enabled = pDmaBuffer[0];    // 0: screenshots VBL enabled?
    screenShots.take    = pDmaBuffer[1];    // 1: take screenshot?

    //-------------
    // now take screenshot if requested
    if(!screenShots.take) {                 // don't take screenshot? quit
        hdIf.forceFlock = 0;                // force FLOCK back to normal
        return;
    }
    
    sendScreenShot();                       // send screenshot
    hdIf.forceFlock = 0;                    // force FLOCK back to normal
}
  
void sendScreenShot(void)
{
	WORD *pxPal         =    (WORD*)  0xffff8240;
	BYTE *pxScreen      =   (BYTE *) (*((DWORD*) 0x44e));
	BYTE  screenMode    = (*((BYTE*)  0xffff8260)) & 3;
 
    //---------------------------
    // send screen memory 
	writeScreen(TRAN_CMD_SENDSCREENCAST, screenMode, pxScreen, 32000);
	
    //---------------------------
    // send 16 ST palette entries 
    #define ST_PALETTE_SIZE     (16 * 2)
    
	memcpy(pDmaBuffer, pxPal, ST_PALETTE_SIZE);                 // copy palette from Video chip to RAM
    
    static BYTE prevPal[ST_PALETTE_SIZE] = { 0 };               // previous palette which was sent
    int changed = memcmp(pDmaBuffer, prevPal, ST_PALETTE_SIZE); // see if pallete changed since last time

    if(changed) {                                               // palette changed, send it
        memcpy(prevPal, pDmaBuffer, ST_PALETTE_SIZE);           // make copy of this palette, so we won't send it next time if it won't change
        writeScreen(TRAN_CMD_SCREENCASTPALETTE, screenMode, pDmaBuffer, ST_PALETTE_SIZE);
    }
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
	
	(*hdIf.cmd)(ACSI_WRITE, commandLong, CMD_LENGTH_LONG, bfr, sectorCount);	// send command to host over ACSI 
} 
