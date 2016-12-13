#include <mint/osbind.h> 
#include "acsi.h"
#include "gemdos.h"
#include "gemdos_errno.h"
#include "translated.h"
#include "stdlib.h"

#include "screen.h"

extern BYTE commandShort[CMD_LENGTH_SHORT]; 
extern BYTE commandLong[CMD_LENGTH_LONG]; 

extern WORD _vblskipscreen;
extern WORD _vblskipconfig;
extern BYTE *pBuffer;
extern BYTE deviceID;

DWORD writeScreen(BYTE command, BYTE screenmode, BYTE *bfr, DWORD cnt);
int getConfig(void);
  
void screenworker()
{
	/* save DMA address - in case some DMA using programm is resuing its set DMA address */
	DWORD dmaadr=getdma();
	WORD* pxPal     = (WORD *)            0xffff8240;
	BYTE* pxScreen  = (BYTE *) *((DWORD*) 0x44e);
	/* send screen memory */
	writeScreen(TRAN_CMD_SENDSCREENCAST,(*((BYTE*)0xffff8260))&3, (BYTE *) pxScreen,32000);
	/* send 16 ST palette entries */
	memcpy(pBuffer,pxPal,16*2);
	writeScreen(TRAN_CMD_SCREENCASTPALETTE,(*((BYTE*)0xffff8260))&3, (BYTE *) pBuffer,16*2);
	/* set old DMA address */
	setdma(dmaadr);
}  
   
void configworker()
{
	/* save DMA address - in case some DMA using programm is resuing its set DMA address */
	DWORD dmaadr=getdma();
	if( getConfig()>=0 )
	{
		_vblskipscreen=(unsigned int)pBuffer[24];
		/* ensure safe values */
		if( _vblskipscreen<2 ){
			_vblskipscreen=2;
		}    
		if( _vblskipscreen>255 ){
			_vblskipscreen=255;
		}    
	}
	/* set old DMA address */
	setdma(dmaadr);
}  

DWORD writeScreen(BYTE command, BYTE screenmode, BYTE *bfr, DWORD cnt)
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
	
	BYTE res = acsi_cmd(ACSI_WRITE, commandLong, CMD_LENGTH_LONG, bfr, sectorCount);	// send command to host over ACSI 
	
	/* if all data transfered, return count of all data */
	if(res == RW_ALL_TRANSFERED) {
		return cnt;
	}

	/* if the result is also not partial transfer, then some other error happened, return that no data was transfered */
	if(res != E_OK ) {
		return 0;	
	}
	
	return 0;
} 

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