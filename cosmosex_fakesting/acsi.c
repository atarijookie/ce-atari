//--------------------------------------------------
#include <mint/osbind.h>
#include "stdlib.h"

#include "acsi.h"
#include "stdlib.h"

// -------------------------------------- 
// the following variables are global ones, because the acsi_cmd() could be called from user mode, so the params will be stored to these global vars and then the acsi_cmd_supervisor() will handle that...
BYTE  gl_ReadNotWrite;
BYTE *gl_cmd;
BYTE  gl_cmdLength;
BYTE *gl_buffer;
WORD  gl_sectorCount;

static BYTE acsi_cmd_supervisor(void);

extern WORD fromVbl;                    // this is non-zero when acsi_cmd is called from VBL (no need for Supexec() then)
// -------------------------------------- 
// call this from user mode
BYTE acsi_cmd(BYTE ReadNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount)
{
    // store params to global variables
    gl_ReadNotWrite = ReadNotWrite;
    gl_cmd          = cmd;
    gl_cmdLength    = cmdLength;
    gl_buffer       = buffer;
    gl_sectorCount  = sectorCount;

    BYTE ret;
    if(fromVbl) {
        ret = acsi_cmd_supervisor();
    } else {
        // call the routine which needs to be in supervisor
        ret = Supexec(acsi_cmd_supervisor);
    }
    
    return ret;
}

// -------------------------------------- 
// call the following from supervisor, with all the gl_ vars set
static BYTE acsi_cmd_supervisor(void)
{
	DWORD status;
	WORD i, wr1, wr2;

    DWORD end = getTicks_fromSupervisor() + 200;    // calculate the terminating tick count, where we should stop looking for unlocked FLOCK
    
    WORD locked;
    while(1) {                                  // while not time out, try again
        locked = *FLOCK;                        // read current lock value
        
        if(!locked) {                           // if not locked, lock and continue
            *FLOCK = -1;                        // disable FDC operations 
            break;
        }
        
        if(getTicks_fromSupervisor() >= end) {  // on time out - fail, return ACSIERROR
            return ACSIERROR;
        }
    }
    
	setdma((DWORD) gl_buffer);                  // setup DMA transfer address 

	//*******************************
	// transfer 0th cmd byte 
	*dmaAddrMode = NO_DMA | HDC;              	// write 1st byte (0) with A1 low 
	*dmaAddrData = gl_cmd[0];
	*dmaAddrMode = NO_DMA | HDC | A0;         	// A1 high again 

	if (qdone() != OK) {					    // wait for ack 
		hdone();                          	    // restore DMA device to normal 

		return ACSIERROR;
	}
	//*******************************
	// transfer middle cmd bytes 
	for(i=1; i<(gl_cmdLength-1); i++) {
		*dmaAddrData = gl_cmd[i];
		*dmaAddrMode = NO_DMA | HDC | A0;
	
		if (qdone() != OK) {				    // wait for ack 
			hdone();                            // restore DMA device to normal 
			
			return ACSIERROR;
		}
	}
	
	// wr1 and wr2 are defined so we could toggle R/W bit and then setup Read / Write operation  
	if(gl_ReadNotWrite==1) {						
		wr1 = DMA_WR;
		wr2 = 0;
	} else {
		wr1 = 0;
		wr2 = DMA_WR;
	}

    *dmaAddrMode = wr1 | NO_DMA | SC_REG;           // clear FIFO = toggle R/W bit 
    *dmaAddrMode = wr2 | NO_DMA | SC_REG;           // and select sector count reg  

    *dmaAddrSectCnt = gl_sectorCount;               // write sector cnt to DMA device 
    *dmaAddrMode = wr2 | NO_DMA | HDC | A0;         // select DMA data register again 

    *dmaAddrData = gl_cmd[gl_cmdLength - 1];        // transfer the last command byte              
    *dmaAddrMode = wr2;                             // start DMA transfer 

    status = endcmd(wr2 | NO_DMA | HDC | A0);       // wait for DMA completion 
	hdone();                                        // restore DMA device to normal 

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

	*dmaAddrMode = NO_DMA;        	// restore DMA mode register 
	*FLOCK = 0;                 		// FDC operations may get going again 
	
	val = *dmaAddrStatus;
	return val;						// read and return DMA status register 
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
	until = t_ticks + now;   			// calc value timer must get to 

	while(1) {
		gpip = *mfpGpip;
		
		if ((gpip & IO_DINT) == 0) {	// Poll DMA IRQ interrupt 
			return OK;                 	// got interrupt, then OK 
		}

		now = *HZ_200;
		
		if(now >= until) {
			break;
		}
	}

	return ACSIERROR;                  // no interrupt, and timer expired, 
}
//**************************************************************************


