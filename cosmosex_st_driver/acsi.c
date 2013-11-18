/*--------------------------------------------------*/
/* #include <tos.h> */
#include <mint/sysbind.h>

#include "acsi.h"

/* -------------------------------------- */
BYTE acsi_cmd(BYTE ReadNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount)
{
	DWORD status;
	WORD i, wr1, wr2;
	void *OldSP;

	OldSP = (void *) Super((void *)0);  	/* supervisor mode */ 

	FLOCK = -1;                            	/* disable FDC operations */
	setdma((DWORD) buffer);                 /* setup DMA transfer address */

	/*********************************/
	/* transfer 0th cmd byte */
	*dmaAddrMode = NO_DMA | HDC;              	/* write 1st byte (0) with A1 low */
	*dmaAddrData = cmd[0];
	*dmaAddrMode = NO_DMA | HDC | A0;         	/* A1 high again */

	if (qdone() != OK) {					/* wait for ack */
		hdone();                          	/* restore DMA device to normal */
		
		Super((void *)OldSP);  			    /* user mode */
		return ERROR;
	}

	/*********************************/
	/* transfer middle cmd bytes */
	for(i=1; i<(cmdLength-1); i++) {
		*dmaAddrData = cmd[i];
		*dmaAddrMode = NO_DMA | HDC | A0;
	
		if (qdone() != OK) {				/* wait for ack */
			hdone();                        /* restore DMA device to normal */
			
			Super((void *)OldSP);  			    /* user mode */
			return ERROR;
		}
	}

	/* wr1 and wr2 are defined so we could toggle R/W bit and then setup Read / Write operation */ 
	if(ReadNotWrite==1) {						
		wr1 = DMA_WR;
		wr2 = 0;
	} else {
		wr1 = 0;
		wr2 = DMA_WR;
	}
  
    *dmaAddrMode = wr1 | NO_DMA | SC_REG;  		/* clear FIFO = toggle R/W bit */
    *dmaAddrMode = wr2 | NO_DMA | SC_REG;          /* and select sector count reg */ 

    *dmaAddrSectCnt = sectorCount;				/* write sector cnt to DMA device */
    *dmaAddrMode = wr2 | NO_DMA | HDC | A0;        /* select DMA data register again */

    *dmaAddrData = cmd[cmdLength - 1];      		/* transfer the last command byte */             
    *dmaAddrMode = wr2;                         	/* start DMA transfer */

    status = endcmd(wr2 | NO_DMA | HDC | A0);   /* wait for DMA completion */

	hdone();                                	/* restore DMA device to normal */
	
	Super((void *)OldSP);  			    		/* user mode */
	return status;
}
/****************************************************************************/
long endcmd(WORD mode)
{
 if (fdone() != OK)                   /* wait for operation done ack */
    return(ERRORL);

 *dmaAddrMode = mode;                    /* write mode word to mode register */
 return((long)((*dmaAddrData) & 0x00FF));  /* return completion byte */
}
/****************************************************************************/
long hdone(void)
{
 *dmaAddrMode = NO_DMA;        /* restore DMA mode register */
 FLOCK = 0;                 /* FDC operations may get going again */
 return((long)*dmaAddrStatus); /* read and return DMA status register */
}
/****************************************************************************/
void setdma(DWORD addr)
{
	*dmaAddrLo	= (BYTE)(addr);
	*dmaAddrMid	= (BYTE)(addr >> 8);
	*dmaAddrHi	= (BYTE)(addr >> 16);
}
/****************************************************************************/
long qdone(void)
{
	return(wait_dma_cmpl(STIMEOUT));
}
/****************************************************************************/
long fdone(void)
{
	return(wait_dma_cmpl(LTIMEOUT));
}
/****************************************************************************/
long wait_dma_cmpl(DWORD t_ticks)
{
	DWORD to_count;
	BYTE *mfpGpip = (BYTE *) 0xFFFA01;
	BYTE gpip;
 
	to_count = t_ticks + HZ_200;   /* calc value timer must get to */

	do {
		gpip = *mfpGpip;
		
		if ((gpip & IO_DINT) == 0) {	/* Poll DMA IRQ interrupt */
			return(OK);                 /* got interrupt, then OK */
		}

	}  while (HZ_200 <= to_count);      /* check timer */

	return(ERROR);                      /* no interrupt, and timer expired, */
}
/****************************************************************************/


