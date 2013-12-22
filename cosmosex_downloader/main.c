#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <unistd.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "acsi.h"
#include "main.h"

BYTE ce_findId(void);
BYTE ce_identify(void);

BYTE dmaBuffer[DMA_BUFFER_SIZE + 2];
BYTE *pDmaBuffer;

BYTE deviceID;

BYTE commandShort2[CMD_LENGTH_SHORT]	= {   0, 0, 0, 0, 0, 0};
BYTE commandLong2[CMD_LENGTH_LONG]		= {0x1f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

BYTE getSector(void);

/* ------------------------------------------------------------------ */
int main( int argc, char* argv[] )
{
	int i;

	/* write some header out */
	(void) Clear_home();
	(void) Cconws("\33p[ CEDD downloader ]\33q\r\n\r\n");

	/* create buffer pointer to even address */
	pDmaBuffer = &dmaBuffer[2];
	pDmaBuffer = (BYTE *) (((DWORD) pDmaBuffer) & 0xfffffffe);		/* remove odd bit if the address was odd */

	/* now set up the acsi command bytes so we don't have to deal with this one anymore */
	WORD sectorCount = 30;
	deviceID = 0;

	commandShort2[0] = (deviceID << 5) | 0x08;			// read sector command
	commandShort2[4] = 1;								// sector count
	
	Fdelete("A:\\CEDD.PRG");
	
	WORD f = Fcreate("A:\\CEDD.PRG", 0);
	
	if(f <= 0) {
		(void) Cconws("Failed to create A:\\CEDD.PRG\r\n");
		sleep(3);
		return 0;
	}
	
	for(i=0; i<sectorCount; i++) {
		commandShort2[3] = i;							// sector index
		Supexec(getSector);
	
		Fwrite(f, 512, pDmaBuffer);
	}
	
	Fclose(f);

	(void) Cconws("CEDD was saved as A:\\CEDD.PRG\r\n");
	sleep(3);
	
	return 0;		
}

BYTE getSector(void)
{
	WORD res;

	memset(pDmaBuffer, 0, 512);              									/* clear the buffer */
	res = acsi_cmd(ACSI_READ, commandShort2, CMD_LENGTH_SHORT, pDmaBuffer, 1);	/* issue the command and check the result */

	if(res != OK) {                        										/* if failed, return FALSE */
		return 0;
	}

	return 1;                             										/* success */
}




