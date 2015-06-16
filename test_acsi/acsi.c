/*--------------------------------------------------*/
#ifdef ONPC
    #include <mint/osbind.h>
    #include "stdlib.h"
#endif

#include <mint/sysbind.h>

#include "acsi.h"

#ifdef ONPC
    BYTE getCharSerial(BYTE *val);
    void sendCharSerial(BYTE val);
    void waitForMarker(void);

    //#define DEVICE_ID       DEV_PRINTER
    #define DEVICE_ID       DEV_AUX

#endif

/* -------------------------------------- */
BYTE acsi_cmd(BYTE ReadNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount)
{
///////////////////////////////////////////////////////////////////////////////
#ifndef ONPC
	DWORD status;
	WORD i, wr1, wr2;

	*FLOCK = -1;                            	/* disable FDC operations */
	setdma((DWORD) buffer);                 /* setup DMA transfer address */

	/*********************************/
	/* transfer 0th cmd byte */
	*dmaAddrMode = NO_DMA | HDC;              	/* write 1st byte (0) with A1 low */
	*dmaAddrData = cmd[0];
	*dmaAddrMode = NO_DMA | HDC | A0;         	/* A1 high again */

	if (qdone() != OK) {					/* wait for ack */
		hdone();                          	/* restore DMA device to normal */

		return ACSIERROR;
	}
	/*********************************/
	/* transfer middle cmd bytes */
	for(i=1; i<(cmdLength-1); i++) {
		*dmaAddrData = cmd[i];
		*dmaAddrMode = NO_DMA | HDC | A0;
	
		if (qdone() != OK) {				/* wait for ack */
			hdone();                        /* restore DMA device to normal */
			
			return ACSIERROR;
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

	return status;

#else   ///////////////////////////////////////////////////////////////////////////////

    // create header
    sendCharSerial(0xfe);
    sendCharSerial(ReadNotWrite);
    
    WORD i;
    for(i=0; i<cmdLength; i++) {
        sendCharSerial(cmd[i]);
    }
    
    for(i=0; i<(14-cmdLength); i++) {
        sendCharSerial(0);
    }
    
    sendCharSerial(cmdLength);
    sendCharSerial(sectorCount >> 8);
    sendCharSerial(sectorCount);
    
    BYTE *pBfr;

    // now read or write the data
    if(ReadNotWrite) {
        int cnt = sectorCount * 512;
        pBfr = buffer;
        
        waitForMarker();
        
        for(i=0; i<cnt; i++) {
            BYTE res = getCharSerial(&pBfr[i]);

            if(!res) {
                return 0;
            }
        }
    } else {
        int cnt = sectorCount * 512;
        pBfr = buffer;
    
        for(i=0; i<cnt; i++) {
            sendCharSerial(pBfr[i]);
        }
        
        waitForMarker();
    }
    
    BYTE status;
    getCharSerial(&status);
    
    return status;
#endif
}

#ifdef ONPC

void waitForMarker(void)
{
    while(1) {
        BYTE marker = 0;
        getCharSerial(&marker);
        
        if(marker == 0xfe) {
            break;
        }        
    }
}

void sendCharSerial(BYTE val)
{
    while(1) {
        BYTE res = Bcostat(DEVICE_ID);
        
        if(res != 0) {
            break;
        }
    }
    
    Bconout(DEVICE_ID, val);
}

BYTE getCharSerial(BYTE *val)
{
    DWORD start = getTicks();

    while(1) {                              // wait while the device is able to provide data
/*
        DWORD now = getTicks();
                
        if((now - start) > 100) {           // if it takes more than 0.5 seconds
            *val = 0;
            return FALSE;                   // fail
        }
*/            
        BYTE stat = Bconstat(DEVICE_ID);      // get device status
            
        if(stat != 0) {                     // can receive? break
            break;
        }
    }
          
    *val = Bconin(DEVICE_ID);                 // get it
    return TRUE;                               // success
}

#endif

/****************************************************************************/
BYTE endcmd(WORD mode)
{
	WORD val;

	if (fdone() != OK)                  /* wait for operation done ack */
		return ACSIERROR;

	*dmaAddrMode = mode;                /* write mode word to mode register */

	val = *dmaAddrData;
	val = val & 0x00ff;

	return val;							/* return completion byte */
}
/****************************************************************************/
BYTE hdone(void)
{
	WORD val;

	*dmaAddrMode = NO_DMA;        	/* restore DMA mode register */
	*FLOCK = 0;                 		/* FDC operations may get going again */
	
	val = *dmaAddrStatus;
	return val;						/* read and return DMA status register */
}
/****************************************************************************/
void setdma(DWORD addr)
{
	*dmaAddrLo	= (BYTE)(addr);
	*dmaAddrMid	= (BYTE)(addr >> 8);
	*dmaAddrHi	= (BYTE)(addr >> 16);
}
/****************************************************************************/
DWORD getdma()
{
	DWORD addr;
	addr=(*dmaAddrLo)|((*dmaAddrMid)<<8)|((*dmaAddrHi)<<16);
	return addr;
}
/****************************************************************************/
BYTE qdone(void)
{
	return wait_dma_cmpl(STIMEOUT);
}
/****************************************************************************/
BYTE fdone(void)
{
	return wait_dma_cmpl(LTIMEOUT);
}
/****************************************************************************/
BYTE wait_dma_cmpl(DWORD t_ticks)
{
	DWORD now, until;
	BYTE gpip;
     
	now = *HZ_200;
	until = t_ticks + now;   			/* calc value timer must get to */
	                                            
	while(1) {
		gpip = *mfpGpip;
		
		if ((gpip & IO_DINT) == 0) {	/* Poll DMA IRQ interrupt */
			return OK;                 	/* got interrupt, then OK */
		}
                           
		now = *HZ_200;
		
		if(now >= until) {
			break;
		}
	}

	return ACSIERROR;                  /* no interrupt, and timer expired, */
}
/****************************************************************************/


