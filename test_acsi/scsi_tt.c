// based on AHDI 6.061 sources

#include "scsi.h"

#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <support.h>

#include <stdint.h>
#include <stdio.h>



//-----------------
// local function definitions
static BYTE sblkscsi(BYTE scsiId, BYTE *cmd, BYTE cmdLength, BYTE *dataAddr, DWORD dataCount);
static BYTE selscsi(BYTE scsiId);
static WORD dataTransfer(BYTE readNotWrite, BYTE *bfr, DWORD byteCount, BYTE cmdLength);
static void resetscsi(void);
static int  w4stat(void);
static BYTE doack(void);
       DWORD setscstmout(void);
       BYTE  w4req(void);

int PIO_read(void);
int PIO_write(BYTE data);

#define SCDMA 
       
#ifdef SCDMA
static BYTE dmaDataTransfer(BYTE readNotWrite, BYTE cmdLength);
static BYTE w4int(void);
void   setDmaAddr_TT(DWORD addr);
DWORD  getDmaAddr_TT(void);
void   setDmaCnt_TT(DWORD dataCount); 
#else 
static WORD pioDataTransfer(BYTE readNotWrite, BYTE *bfr, DWORD byteCount);
static WORD pioDataTransfer_read(BYTE *bfr, DWORD byteCount);
static WORD pioDataTransfer_write(BYTE *bfr, DWORD byteCount);
#endif

extern BYTE machine;

DWORD scsi_getReg_TT(int whichReg);
void  scsi_setReg_TT(int whichReg, DWORD value);
void  scsi_clrBit_TT(int whichReg, DWORD bitMask);
void  scsi_setBit_TT(int whichReg, DWORD bitMask);
//-----------------

void clearCache030(void);

static DWORD ttCmdTimeOut;              // timeout time for scsi_cmd_TT() from start to end
BYTE scsi_cmd_TT(BYTE readNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount)
{
    //------------
    // first we start by extracting ID and fixing the cmd[] array because there's different format of this for ACSI and SCSI
    BYTE scsiId = (cmd[0] >> 5);        // get only drive ID bits

    cmd[0] = cmd[0] & 0x1f;             // remove possible drive ID bits
    
    if((cmd[0] & 0x1f) == 0x1f) {       // if it's ICD format of command, skip the 0th byte
        cmd++;
        cmdLength--;
    }
    //------------
    ttCmdTimeOut = setscstmout();       // set up a short timeout
    BYTE res = sblkscsi(scsiId, cmd, cmdLength, buffer, sectorCount * 512);      // send command block

    if(res) {
        (void) Cconws("scsi_cmd_tt failed on sblkscsi \r\n");
        return -1;
    }
    
    if(sectorCount != 0) {
        DWORD byteCount = sectorCount * 512;
        WORD wres = dataTransfer(readNotWrite, buffer, byteCount, cmdLength);
        
        if(wres) {
            (void) Cconws("scsi_cmd_tt failed on dataTransfer \r\n");
            return -1;
        }
    }
    
    BYTE status;
    status = w4stat();          // wait for status byte

    if(status) {
        (void) Cconws("scsi_cmd_tt failed on w4stat \r\n");
    }
    
    return status;
}

WORD dataTransfer(BYTE readNotWrite, BYTE *bfr, DWORD byteCount, BYTE cmdLength)
{
    WORD res;

    scsi_setReg_TT(REG_ICR, 0);                // deassert the data bus
    
    if(readNotWrite) {                              // read
        scsi_setReg_TT(REG_TCR, TCR_PHASE_DATA_IN);    // set DATA IN  phase
    } else {                                        // write
        scsi_setReg_TT(REG_TCR, TCR_PHASE_DATA_OUT);   // set DATA OUT phase
    }
    
    res = scsi_getReg_TT(REG_REI);             // clear potential interrupt
    
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
    scsi_setReg_TT(REG_MR, 2);             // enable DMA mode
    scsi_setReg_TT(REG_DIR, 0);            // start the DMA receive
    
    if(readNotWrite) {                          // on read
        scsi_setReg_TT(REG_DMACTL, DMAIN);         // set the DMAC direction to IN
        scsi_setReg_TT(REG_DMACTL, DMAIN+DMAENA);  // turn on DMAC
    } else {                                    // on write
        scsi_setReg_TT(REG_DMACTL, DMAOUT);        // set the DMAC direction to IN
        scsi_setReg_TT(REG_DMACTL, DMAOUT+DMAENA); // turn on DMAC
    }
    
    BYTE res;
    res = w4int();                                  // wait for int
    if(res) {
        (void) Cconws(" dmaDataTansfer() failed - w4int() timeout\r\n");
        return -1;
    }
    
    if(!readNotWrite) {                 // for WRITE the code end here, nothing more to do, just return good result
        return 0;
    }

    //--------------------------
    // the rest is only for the case of DMA read 

	clearCache030();

    BYTE rest = *bSDMAPTR_lo;   // see if this was an odd transfer
    rest = rest & 0x03;         // get only 2 lowest bits
    
    if(rest == 0) {             // transfer size was multiple of 4? Great, finish.
        return 0;
    }
    
    //----------------
    // the following code is only for case if the DMA read size (count) was not multiple of 4
    DWORD dmaPtr;
    BYTE *pData;
    dmaPtr  = getDmaAddr_TT();
    dmaPtr  = dmaPtr & 0xfffffffc;  // where does data go to?
    pData   = (BYTE *) dmaPtr;      // int to pointer

    DWORD residue = scsi_getReg_TT(REG_DMARES);    // get the remaining bytes

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

#ifndef SCDMA

WORD pioDataTransfer(BYTE readNotWrite, BYTE *bfr, DWORD byteCount)
{
    WORD res;
    
    if(readNotWrite) {          // read?
        res = pioDataTransfer_read(bfr, byteCount);
    } else {                    // write?
        res = pioDataTransfer_write(bfr, byteCount);
    }
    
    return res;                 // good
}

WORD pioDataTransfer_read(BYTE *bfr, DWORD byteCount)
{
    int i, res;
    
    for(i=0; i<byteCount; i++) {
        res = PIO_read();
            
        if(res == -2) {         // phase changed? pretend no error
            (void) Cconws("pioDataTransfer - phase changed while read\n\r");
            return 0;
        }            
        
        if(res < 0) {           // other error? quit
            return -1;
        }
        
        bfr[i] = (BYTE) res;
    }
    
    return 0;                       // good
}

WORD pioDataTransfer_write(BYTE *bfr, DWORD byteCount)
{
    int i, res;
    
    for(i=0; i<byteCount; i++) {
        res = PIO_write(bfr[i]);

        if(res == -2) {         // phase changed? pretend no error
            (void) Cconws("pioDataTransfer - phase changed while write\n\r");
            return 0;
        }            
        
        if(res) {
            return -1;
        }
    }
    
    return 0;                       // good
}

#endif    
    
// sblkscsi() - set DMA pointer and count and send command block
BYTE sblkscsi(BYTE scsiId, BYTE *cmd, BYTE cmdLength, BYTE *dataAddr, DWORD dataCount)
{
    BYTE res;
    
    res = selscsi(scsiId);  // select required device
    
    if(res) {               // if failed, quit with failure
        (void) Cconws("sblkscsi failed on selscsi\r\n");
        return -1;
    }

#ifdef SCDMA    
    // set DMA pointer to buffer address
    setDmaAddr_TT((DWORD) dataAddr);

    // set DMA count
    setDmaCnt_TT(dataCount);
#endif
    
    scsi_setReg_TT(REG_TCR, TCR_PHASE_CMD);                // set COMMAND PHASE (assert C/D)
    scsi_setReg_TT(REG_ICR, 1);                            // assert data bus

    int i;
    
    for(i=0; i<cmdLength; i++) {                        // send all the cmd bytes using PIO
        res = PIO_write(cmd[i]);
        
        if(res) {                                       // if time out happened, fail
            (void) Cconws("sblkscsi failed on PIO_write\r\n");
            return -1;
        }
    }
    
    return 0;
}

// Selects the SCSI device with specified SCSI ID
BYTE selscsi(BYTE scsiId)
{
    BYTE res;

    while(1) {                                          // STILL busy from last time?
        BYTE icr = scsi_getReg_TT(REG_CR);
        if((icr & ICR_BUSY) == 0) {                     // if not, it's available
            break;
        }
        
        DWORD now = *HZ_200;
        if(now >= ttCmdTimeOut) {                        // if time out, fail
            return -1;
        }        
    }
    
    scsi_setReg_TT(REG_TCR, TCR_PHASE_DATA_OUT);           // data out phase
    scsi_setReg_TT(REG_ISR, 0);                            // no interrupt from selection
    scsi_setReg_TT(REG_ICR, 0x0c);                         // assert BSY and SEL

    BYTE selId  = (1 << scsiId);                           // convert number of device to bit 
    scsi_setReg_TT(REG_ODR, selId);                        // set dest SCSI IDs
    
    scsi_setReg_TT(REG_ICR, 0x0d);                         // assert BUSY, SEL and data bus
    scsi_clrBit_TT(REG_MR, MR_ARBIT);                      // clear arbitrate bit
    scsi_clrBit_TT(REG_ICR, (1 << 3));                     // clear BUSY
    
    while(1) {                          // wait for busy bit to appear
        BYTE icr = scsi_getReg_TT(REG_CR);
        
        if(icr & ICR_BUSY) {            // if bit set, good
            res = 0;
            break;
        }
        
        DWORD now = *HZ_200;
        if(now >= ttCmdTimeOut) {                       // if time out, fail
            res = -1;
            break;
        }        
    }
    
    scsi_setReg_TT(REG_ICR, 0);                        // clear SEL and data bus assertion
    return res;
}    

void resetscsi(void)
{
    scsi_setReg_TT(REG_ICR, 0x80);                     // assert RST

    ttCmdTimeOut = setscstmout();
    
    DWORD now;
    while(1) {                          // wait 500 ms
        now = *HZ_200;
        
        if(now >= ttCmdTimeOut) {        
            break;
        }        
    }
    
    scsi_setReg_TT(REG_ICR, 0);
    
    ttCmdTimeOut = setscstmout();        

    while(1) {                          // wait 1000 ms
        now = *HZ_200;
        
        if(now >= ttCmdTimeOut) {        
            break;
        }        
    }
}
    
// w4int - wait for interrupts from 5380 or DMAC during DMA transfer
// Comments:
//	When 5380 is interrupted, it indicates a change of data to status phase (i.e., DMA is done), or ...
//	When DMAC is interrupted, it indicates either DMA count is zero, or there is an internal bus error.
BYTE w4int(void)
{
    BYTE res;
    
    while(1) {
        res = *MFP2;
        if(res & GPIP2SCSI) {           // NCR 5380 interrupt? 
            break;
        }
        
        if((res & GPIP25) == 0) {       // DMAC interrupt? 
            WORD wres = scsi_getReg_TT(REG_DMACTL);    // get the DMAC status
            if(wres & 0x80) {           // check for bus err/ignore cntout ints
                return -1;
            }            
        }
    
        DWORD now = *HZ_200;
        if(now >= ttCmdTimeOut) {           // time out? fail
            return -1;
        }
    }
    
    scsi_getReg_TT(REG_REI);                // clear potential interrupt
    scsi_setReg_TT(REG_DMACTL, DMADIS);    // disable DMA
    scsi_setReg_TT(REG_MR,  0);            // disable DMA mode
    scsi_setReg_TT(REG_ICR, 0);            // make sure data bus is not asserted

    return 0;
}

// w4stat - wait for status byte and message byte.
int w4stat(void)
{
    int  iRes;
    
	scsi_setReg_TT(REG_TCR, TCR_PHASE_STATUS);     // STATUS IN phase
	scsi_getReg_TT(REG_REI);                 // clear potential interrupt

    //-----------------
    // receive status byte
    iRes = PIO_read();
    if(iRes < 0) {
        (void) Cconws("w4stat failed on reading status byte \r\n");
        return -1;
    }
    BYTE status = iRes;

    //-----------------
    // receive message byte
	scsi_setReg_TT(REG_TCR, TCR_PHASE_MESSAGE_IN);     // MESSAGE IN phase
	scsi_getReg_TT(REG_REI);                 // clear potential interrupt

    iRes = PIO_read();
    if(iRes < 0) {
        (void) Cconws("w4stat failed on reading message in \r\n");
        return -1;
    }
    
    return status;
}

int PIO_read(void)
{
    BYTE res;

    res = w4req();                      // wait for status byte
    if(res) {                           // if timed-out, fail
        return -1;
    }
    
    res = scsi_getReg_TT(REG_DSR);
    if((res & (1 << 3)) == 0) {         // PHASE MATCH bit from BUS AND STATUS REGISTER is low? SCSI phase changed
        return -2;
    }
   
    BYTE data = scsi_getReg_TT(REG_DB); // get the status byte

    res = doack();                      // signal that status byte is here
    if(res) {                           // if timed-out, fail
        return -1;
    }

    return data;
} 

int PIO_write(BYTE data)
{
    BYTE res;

    res = w4req();                      // wait for status byte
    if(res) {                           // if timed-out, fail
        return -1;
    }

    res = scsi_getReg_TT(REG_DSR);
    if((res & (1 << 3)) == 0) {         // PHASE MATCH bit from BUS AND STATUS REGISTER is low? SCSI phase changed
        return -2;
    }
    
    scsi_setReg_TT(REG_DB, data);

    res = doack();                      // signal that status byte is here
    if(res) {                           // if timed-out, fail
        return -1;
    }

    return 0;
} 
            
// w4req() - wait for REQ to come during hand shake of non-data bytes
BYTE w4req(void) 
{
    while(1) {                      // wait for REQ
        BYTE icr = scsi_getReg_TT(REG_CR);
        if(icr & ICR_REQ) {         // if REQ appeared, good
            return 0;
        }
        
        DWORD now = *HZ_200;
        if(now >= ttCmdTimeOut) {   // if time out, fail
            break;
        }
    }
    
    return -1;                      // time out
}

// doack() - assert ACK
BYTE doack(void)
{
    scsi_setBit_TT(REG_ICR, ICR_ACK | ICR_DBUS);      // assert ACK (and data bus)

    BYTE res;
    
    while(1) {
        BYTE icr = scsi_getReg_TT(REG_ICR);
        if((icr & ICR_REQ) == 0) {      // if REQ gone, good
            res = 0;
            break;
        }
        
        DWORD now = *HZ_200;
        if(now >= ttCmdTimeOut) {       // if time out, fail
            res = -1;
            break;
        }
    }

    scsi_clrBit_TT(REG_ICR, ICR_ACK);   // clear ACK
    return res;
}

// setscstmout - set up a timeout count for the SCSI for SCSTMOUT long
DWORD setscstmout(void)
{
    DWORD now = *HZ_200;
    return (now + scltmout);
}

void setDmaAddr_TT(DWORD addr)
{
    *bSDMAPTR_lo        = (BYTE) (addr      );
    *bSDMAPTR_mid_lo    = (BYTE) (addr >>  8);
    *bSDMAPTR_mid_hi    = (BYTE) (addr >> 16);
    *bSDMAPTR_hi        = (BYTE) (addr >> 24);
}

DWORD getDmaAddr_TT(void)
{
    DWORD  dmaPtr;
    dmaPtr = ((*bSDMAPTR_hi) << 24) | ((*bSDMAPTR_mid_hi) << 16) | ((*bSDMAPTR_mid_lo) << 8) | (*bSDMAPTR_lo);
    return dmaPtr;
}

void setDmaCnt_TT(DWORD dataCount) 
{
    *bSDMACNT_hi     = (BYTE) (dataCount >> 24);
    *bSDMACNT_mid_hi = (BYTE) (dataCount >> 16);
    *bSDMACNT_mid_lo = (BYTE) (dataCount >>  8);
    *bSDMACNT_lo     = (BYTE) (dataCount      );
}
//----------------------
// functions for SETING SCSI register
void scsi_setReg_TT(int whichReg, DWORD value)
{
    if(whichReg == REG_DMACTL) {
        *SDMACTL = value;
        return;
    }

    volatile BYTE *pReg = (volatile BYTE *) (0xFFFF8780 + whichReg);
    *pReg = (BYTE) value;
}

//----------------------
// functions for GETTING SCSI register
DWORD scsi_getReg_TT(int whichReg)
{
    if(whichReg == REG_DMARES) {
        return *SDMARES;
    }

    if(whichReg == REG_DMACTL) {
        return *SDMACTL;
    }
    
    volatile BYTE *pReg = (volatile BYTE *) (0xFFFF8780 + whichReg);
    DWORD val = *pReg;
    
    return val;
}

//----------------------
void scsi_setBit_TT(int whichReg, DWORD bitMask)
{
    DWORD val;
    val = scsi_getReg_TT(whichReg);        // read
    val = val | bitMask;                // modify (set bits)
    scsi_setReg_TT(whichReg, val);         // write
}

void scsi_clrBit_TT(int whichReg, DWORD bitMask)
{
    DWORD val;
    DWORD invMask = ~bitMask;
    
    val = scsi_getReg_TT(whichReg);        // read
    val = val & invMask;                // modify (clear bits)
    scsi_setReg_TT(whichReg, val);         // write
}
//----------------------


