//--------------------------------------------------
#include <mint/sysbind.h>
#include "acsi.h"

// -------------------------------------- 
BYTE acsi_cmd(BYTE ReadNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount)
{
	DWORD status;
	WORD i, wr1, wr2;

	*FLOCK = -1;                                // disable FDC operations 
	setdma((DWORD) buffer);                     // setup DMA transfer address 

	//*******************************
	// transfer 0th cmd byte 
	*dmaAddrMode = NO_DMA | HDC;                // write 1st byte (0) with A1 low 
	*dmaAddrData = cmd[0];
	*dmaAddrMode = NO_DMA | HDC | A0;           // A1 high again 

	if (qdone() != OK) {					    // wait for ack 
		hdone();                                // restore DMA device to normal 

		return ACSIERROR;
	}
	//*******************************
	// transfer middle cmd bytes 
	for(i=1; i<(cmdLength-1); i++) {
		*dmaAddrData = cmd[i];
		*dmaAddrMode = NO_DMA | HDC | A0;
	
		if (qdone() != OK) {				    // wait for ack 
			hdone();                            // restore DMA device to normal 
			
			return ACSIERROR;
		}
	}
	
	// wr1 and wr2 are defined so we could toggle R/W bit and then setup Read / Write operation  
	if(ReadNotWrite==1) {						
		wr1 = DMA_WR;
		wr2 = 0;
	} else {
		wr1 = 0;
		wr2 = DMA_WR;
	}

    *dmaAddrMode = wr1 | NO_DMA | SC_REG;       // clear FIFO = toggle R/W bit 
    *dmaAddrMode = wr2 | NO_DMA | SC_REG;       // and select sector count reg  

    *dmaAddrSectCnt = sectorCount;              // write sector cnt to DMA device 
    *dmaAddrMode = wr2 | NO_DMA | HDC | A0;     // select DMA data register again 

    *dmaAddrData = cmd[cmdLength - 1];          // transfer the last command byte              
    *dmaAddrMode = wr2;                         // start DMA transfer 

    status = endcmd(wr2 | NO_DMA | HDC | A0);   // wait for DMA completion 
	hdone();                                    // restore DMA device to normal 

	return status;
}

//**************************************************************************
BYTE endcmd(WORD mode)
{
	WORD val;

	if (fdone() != OK)                  // wait for operation done ack 
		return ACSIERROR;

	*dmaAddrMode = mode;                // write mode word to mode register 

	val = *dmaAddrData;
	val = val & 0x00ff;

	return val;							// return completion byte 
}
//**************************************************************************
BYTE hdone(void)
{
	WORD val;

	*dmaAddrMode = NO_DMA;              // restore DMA mode register 
	*FLOCK = 0;                         // FDC operations may get going again 
	
	val = *dmaAddrStatus;
	return val;                         // read and return DMA status register 
}
//**************************************************************************
void setdma(DWORD addr)
{
	*dmaAddrLo	= (BYTE)(addr);
	*dmaAddrMid	= (BYTE)(addr >> 8);
	*dmaAddrHi	= (BYTE)(addr >> 16);
}
//**************************************************************************
BYTE qdone(void)
{
	return wait_dma_cmpl(STIMEOUT);
}
//**************************************************************************
BYTE fdone(void)
{
	return wait_dma_cmpl(LTIMEOUT);
}
//**************************************************************************
BYTE wait_dma_cmpl(DWORD t_ticks)
{
	DWORD now, until;
	BYTE gpip;
 
	now = *HZ_200;
	until = t_ticks + now;              // calc value timer must get to 

	while(1) {
		gpip = *mfpGpip;
		
		if ((gpip & IO_DINT) == 0) {    // Poll DMA IRQ interrupt 
			return OK;                 	// got interrupt, then OK 
		}

		now = *HZ_200;
		
		if(now >= until) {
			break;
		}
	}

	return ACSIERROR;                   // no interrupt, and timer expired, 
}
//**************************************************************************


