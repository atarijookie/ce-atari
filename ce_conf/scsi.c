// based on AHDI 6.061 sources

#include "scsi.h"

extern BYTE deviceID;

//-----------------
// local function definitions
static BYTE sblkscsi(BYTE *cmd, BYTE cmdLength, BYTE *dataAddr, DWORD dataCount);
static BYTE selscsi(void);
static WORD dataTransfer(BYTE readNotWrite, BYTE *bfr, DWORD byteCount, BYTE cmdLength);
static void resetscsi(void);
static WORD w4stat(void);
static BYTE w4dreq(DWORD timeOutTime);
static BYTE w4req(DWORD timeOutTime);
static BYTE doack(DWORD timeOutTime);
static BYTE hshake(DWORD timeOutTime, BYTE val);
static DWORD setscstmout(void);
static DWORD setscltmout(void);

#ifdef SCDMA
static BYTE dmaDataTransfer(BYTE readNotWrite, BYTE cmdLength);
static BYTE w4int(DWORD timeOutTime);
static DWORD setscxltmout(void);
#else 
static WORD pioDataTransfer(BYTE readNotWrite, BYTE *bfr, DWORD byteCount);
#endif

//-----------------

BYTE scsi_cmd(BYTE readNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount)
{
    BYTE res = sblkscsi(cmd, cmdLength, buffer, sectorCount * 512);      // send command block

    if(res) {
        return -1;
    }
    
    if(sectorCount != 0) {
        DWORD byteCount = sectorCount * 512;
        WORD wres = dataTransfer(readNotWrite, buffer, byteCount, cmdLength);
        
        if(wres) {
            return -1;
        }
    }
    
    BYTE status;
    status = w4stat();          // wait for status byte
    
    return status;
}

WORD dataTransfer(BYTE readNotWrite, BYTE *bfr, DWORD byteCount, BYTE cmdLength)
{
    WORD res;

    *SCSIICR = 0;               // deassert the data bus
    
    if(readNotWrite) {          // read
        *SCSITCR = 1;           // set DATA IN  phase
    } else {
        *SCSITCR = 0;           // set DATA OUT phase
    }
    
    res = *SCSIREI;             // clear potential interrupt
    
#ifdef SCDMA                    // if using DMA for data transfer
    res = dmaDataTransfer(readNotWrite, cmdLength);
#else                           // if using PIO for data transfer
    res = pioDataTransfer(readNotWrite, bfr, byteCount);
#endif

    return res;
}    

#ifdef SCDMA     
BYTE dmaDataTransfer(BYTE readNotWrite, BYTE cmdLength)
{
    // Set up the DMAC for data transfer
    *SCSIMR     = 2;                    // enable DMA mode
    *SCSIDIR    = 0;                    // start the DMA receive
    
    if(readNotWrite) {                  // on read
        *SDMACTL    = DMAIN;            // set the DMAC direction to IN
        *SDMACTL    = DMAIN+DMAENA;     // turn on DMAC
    } else {                            // on write
        *SDMACTL    = DMAOUT;           // set the DMAC direction to IN
        *SDMACTL    = DMAOUT+DMAENA;    // turn on DMAC
    }
    
    DWORD timeOutTime;

    if(cmdLength == 6) {                // short command -> short time out
        timeOutTime = setscstmout();
    } else {                            // long command  -> long timeout
        timeOutTime = setscxltmout();
    }
    
    BYTE res;
    res = w4int(timeOutTime);           // wait for int
    if(res) {
        return -1;
    }
    
    if(!readNotWrite) {                 // for WRITE the code end here, nothing more to do, just return good result
        return 0;
    }

    //--------------------------
    // the rest is only for the case of DMA read 
    
//----------
// TODO: drop instruction and data cache
//	move	sr,-(sp)		// go to IPL 7
//	ori	#$700,sr		// no interrupts right now kudasai
//	movecacrd0			// d0 = (cache control register)
//	ori.w	#$808,d0		// dump both the D and I cache
//	moved0cacr			// update cache control register
//	move	(sp)+,sr		// restore interrupt state
//----------
    
    BYTE rest = *bSDMAPTR_lo;   // see if this was an odd transfer
    rest = rest & 0x03;         // get only 2 lowest bits
    
    if(rest == 0) {             // transfer size was multiple of 4? Great, finish.
        return 0;
    }
    
    //----------------
    // the following code is only for case if the DMA read size (count) was not multiple of 4
    DWORD dmaPtr;
    BYTE *pData;
    dmaPtr  = ((*bSDMAPTR_hi) << 24) | ((*bSDMAPTR_mid_hi) << 16) | ((*bSDMAPTR_mid_lo) << 8) | (*bSDMAPTR_lo);
    dmaPtr  = dmaPtr & 0xfffffffc;  // where does data go to?
    pData   = (BYTE *) dmaPtr;      // int to pointer

    DWORD residue = *SDMARES;       // get the remaining bytes

    int i;
    for(i=0; i<rest; i++) {
        BYTE val;
        val     = residue >> 24;        // get highest byte
        residue = residue << 8;         // shift next byte to highest byte
        
        *pData = val;                   // store byte and move to next position
        pData++;
    }
    
    return 0;                   
}
#endif

WORD pioDataTransfer(BYTE readNotWrite, BYTE *bfr, DWORD byteCount)
{
    DWORD timeOutTime;
    BYTE  val, res;
    
    int i;
    for(i=0; i<byteCount; i++) {
        timeOutTime = setscstmout();
    
        res = w4dreq(timeOutTime);  // wait for REQ
        if(res) {
            return -1;              // fail
        }
        
        res = *SCSIDSR;             
        if((res & (1 << 3)) == 0) { // still in data in phase? no? stop and get status
            break;
        }
        
        if(readNotWrite) {          // read in the data
            val     = *SCSIDB;              
            bfr[i]  = val;
        } else {                    // write the data
            val     = bfr[i];
            *SCSIDB = val;
        }
        
        doack(timeOutTime);         // do ACK the byte
        if(res) {
            return -1;              // fail
        }
    }
    
    return 0;                       // good
}
    
// sblkscsi() - set DMA pointer and count and send command block
BYTE sblkscsi(BYTE *cmd, BYTE cmdLength, BYTE *dataAddr, DWORD dataCount)
{
    BYTE res;
    
    res = selscsi();        // select required device
    
    if(res) {               // if failed, quit with failure
        return -1;
    }

#ifdef SCDMA    
    // set DMA pointer to buffer address
    DWORD addr       = (DWORD) dataAddr;
    *bSDMAPTR_hi     = (BYTE) (addr >> 24);
    *bSDMAPTR_mid_hi = (BYTE) (addr >> 16);
    *bSDMAPTR_mid_lo = (BYTE) (addr >>  8);
    *bSDMAPTR_lo     = (BYTE) (addr      );

    // set DMA count
    *bSDMACNT_hi     = (BYTE) (dataCount >> 24);
    *bSDMACNT_mid_hi = (BYTE) (dataCount >> 16);
    *bSDMACNT_mid_lo = (BYTE) (dataCount >>  8);
    *bSDMACNT_lo     = (BYTE) (dataCount      );
#endif
    
    *SCSITCR = 2;                                       // set COMMAND PHASE (assert C/D)
    *SCSIICR = 1;                                       // assert data bus

    DWORD timeOutTime = setscstmout();                  // set up timeout for sending cmdblk

    int i;
    
    for(i=0; i<cmdLength; i++) {                        // send all the cmd bytes using PIO
        res = hshake(timeOutTime, cmd[i]);
        
        if(res) {                                       // if time out happened, fail
            return -1;
        }
    }
    
    return 0;
}

// Selects the SCSI device with ID stored in deviceID
BYTE selscsi(void)
{
    BYTE res;
    DWORD timeOutTime = setscstmout();  // set up a short timeout

    while(1) {                          // STILL busy from last time?
        BYTE icr = *SCSICR;
        if((icr & ICR_BUSY) == 0) {     // if not, it's available
            break;
        }
        
        DWORD now = *HZ_200;
        if(now >= timeOutTime) {        // if time out, fail
            return -1;
        }        
    }
    
    *SCSITCR = 0;                       // data out phase
    *SCSIISR = 0;                       // no interrupt from selection
    *SCSIICR = 0x0c;                    // assert BSY and SEL

    BYTE selId  = (1 << deviceID);      // convert number of device to bit 
    *SCSIODR     = selId;               // set dest SCSI IDs
    
    *SCSIICR = 0x0d;                    // assert BUSY, SEL and data bus
    *SCSIMR  = (*SCSIMR)    & 0xfe;     // clear arbitrate bit
    *SCSIICR = (*SCSIICR)   & 0xf7;     // clear BUSY
    
    timeOutTime = setscstmout();        // set up for timeout
    
    while(1) {                          // wait for busy bit to appear
        BYTE icr = *SCSICR;
        
        if(icr & ICR_BUSY) {            // if bit set, good
            res = 0;
            break;
        }
        
        DWORD now = *HZ_200;
        if(now >= timeOutTime) {        // if time out, fail
            res = -1;
            break;
        }        
    }
    
    *SCSIICR = 0;                       // clear SEL and data bus assertion
    return res;
}    

void resetscsi(void)
{
    *SCSIICR = 0x80;                    // assert RST

    DWORD timeOutTime = setscstmout();
    
    DWORD now;
    while(1) {                          // wait 500 ms
        now = *HZ_200;
        
        if(now >= timeOutTime) {        
            break;
        }        
    }
    
    *SCSIICR = 0;
    
    timeOutTime = setscltmout();        

    while(1) {                          // wait 1000 ms
        now = *HZ_200;
        
        if(now >= timeOutTime) {        
            break;
        }        
    }
}
    
// w4int - wait for interrupts from 5380 or DMAC during DMA tranfers
// Comments:
//	When 5380 is interrupted, it indicates a change of data to status phase (i.e., DMA is done), or ...
//	When DMAC is interrupted, it indicates either DMA count is zero, or there is an internal bus error.
BYTE w4int(DWORD timeOutTime)
{
    BYTE res;
    timeOutTime += rcaltm;      // add time for recalibration

    while(1) {
        res = *MFP2;
        if(res & GPIP2SCSI) {       // NCR 5380 interrupt? 
            break;
        }
        
        if((res & GPIP25) == 0) {   // DMAC interrupt? 
            WORD wres = *SDMACTL;   // get the DMAC status
            if(wres & 0x80) {       // check for bus err/ignore cntout ints
                resetscsi();        // reset SCSI and exit with failure
                return -1;
            }            
        }
    
        DWORD now = *HZ_200;
        if(now >= timeOutTime) {    // time out? fail   
            return -1;
        }
    }
    
    res         = *SCSIREI;         // clear potential interrupt
    *SDMACTL    = DMADIS;           // disable DMA
    *SCSIMR     = 0;                // disable DMA mode
    *SCSIICR    = 0;                // make sure data bus is not asserted

    return 0;
}

// w4stat - wait for status byte and message byte.
WORD w4stat(void)
{
    BYTE res;
    DWORD timeOutTime;
    
	*SCSITCR = 3;                   // STATUS IN phase
	res = *SCSIREI;                 // clear potential interrupt

    //-----------------
    // receive status byte
    timeOutTime = setscstmout();    // set up time-out for REQ and ACK

    res = w4req(timeOutTime);       // wait for status byte
    if(res) {                       // if timed-out, fail
        return -1;
    }
    
    BYTE status = *SCSIDB;          // get the status byte

    timeOutTime = setscstmout();    // set up time-out for REQ and ACK
    res = doack(timeOutTime);       // signal that status byte is here
    
    if(res) {                       // if timed-out, fail
        return -1;
    }

    //-----------------
    // receive message byte
    
    // TODO:   SCSITCR = ???;       // message in phase 
    
    timeOutTime = setscstmout();    // set up time-out for REQ and ACK

    res = w4req(timeOutTime);       // wait for message byte
    if(res) {                       // if timed-out, fail
        return -1;
    }
  
    BYTE msg __attribute__((unused));
    msg = *SCSIDB;                  // get and ignore message byte

    timeOutTime = setscstmout();    // set up time-out for REQ and ACK
    res = doack(timeOutTime);       // signal that status byte is here
    
    if(res) {                       // if timed-out, fail
        return -1;
    }
    //--------
    
    return status;
}

// w4dreq() - wait for REQ to come during hand shake of data bytes
BYTE w4dreq(DWORD timeOutTime)
{
    return w4req(timeOutTime + rcaltm);
}

// w4req() - wait for REQ to come during hand shake of non-data bytes
BYTE w4req(DWORD timeOutTime) 
{
    while(1) {                      // wait for REQ
        BYTE icr = *SCSICR;
        if(icr & ICR_REQ) {         // if REQ appeared, good
            return 0;
        }
        
        DWORD now = *HZ_200;
        if(now >= timeOutTime) {    // if time out, fail
            break;
        }
    }
    
    return -1;                      // time out
}

// doack() - assert ACK
BYTE doack(DWORD timeOutTime)
{
    BYTE icr    = *SCSIICR;
    icr         = icr | 0x11;           // assert ACK (and data bus)
    *SCSIICR    = icr;

    BYTE res;
    
    while(1) {
        BYTE icr = *SCSICR;
        if((icr & ICR_REQ) == 0) {      // if REQ gone, good
            res = 0;
            break;
        }
        
        DWORD now = *HZ_200;
        if(now >= timeOutTime) {        // if time out, fail
            res = -1;
            break;
        }
    }

    icr         = *SCSIICR;
    icr         = icr & 0xef;           // clear ACK
    *SCSIICR    = icr;
    
    return res;
}

// hshake() - hand shake a byte over to the controller
BYTE hshake(DWORD timeOutTime, BYTE val)
{
    BYTE res;
    res = w4req(timeOutTime);       // wait for REQ to come
    
    if(res) {                       // time out?
        return -1;
    }
    
    *SCSIDB = val;                  // write a byte out to data bus
    res     = doack(timeOutTime);   // assert ACK
    
    return res;
}

// setscstmout - set up a timeout count for the SCSI for SCSTMOUT long
DWORD setscstmout(void)
{
    DWORD now = *HZ_200;
    return (now + scstmout);
}

// setscltmout - set up a timeout count for the SCSI for SCLTMOUT long
DWORD setscltmout(void)
{
    DWORD now = *HZ_200;
    return (now + scltmout);
}

// setscxltmout - set up a timeout count for the SCSI for SCXLTMOUT long
DWORD setscxltmout(void)
{
    DWORD now = *HZ_200;
    return (now + scxltmout);
}



