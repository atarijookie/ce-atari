#include "scsi.h"

extern BYTE deviceID;

void delay(void);
void stopDmaFalcon(void);

static BYTE selscsi_falcon(void);
static BYTE dmaDataWrite_Falcon(BYTE *buffer, WORD sectorCount);
static BYTE dmaDataRead_Falcon(BYTE *buffer, WORD sectorCount);
static BYTE pio_write(BYTE val);
static WORD pio_read(void);
static BYTE getCurrentScsiPhase(void);
static WORD getStatusByte(void);
static void scsi_setReg_Falcon(int whichReg, DWORD value);
static BYTE scsi_getReg_Falcon(int whichReg);
static void scsi_setBit_Falcon(int whichReg, DWORD bitMask);
static void scsi_clrBit_Falcon(int whichReg, DWORD bitMask);
static void setDmaAddr_Falcon(DWORD addr);

BYTE w4req_Falcon(DWORD timeOutTime); 
BYTE doack_Falcon(DWORD timeOutTime);

DWORD setscstmout(void);
BYTE  wait_dma_cmpl(DWORD t_ticks);

void clearCache030(void);

BYTE scsi_cmd_Falcon(BYTE readNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount)
{
    int i;
    BYTE res;
    
    //---------
    // select device
    res = selscsi_falcon();                             // do device selection
    if(res) {
        return -1;
    }
    
    //---------
    // send command
    scsi_setReg_Falcon(REG_TCR, TCR_PHASE_CMD);         // set COMMAND PHASE (assert C/D)
    scsi_setReg_Falcon(REG_ICR, 1);                     // data bus as output

    for(i=0; i<cmdLength; i++) {                        // try to send all cmd bytes
        res = pio_write(cmd[i]);
        
        if(res) {                                       // failed? quit
            return -1;
        }
    }

    //---------
    // transfer data
    if(sectorCount > 0) {
        if(readNotWrite) {
            res = dmaDataRead_Falcon(buffer, sectorCount);
        } else {
            res = dmaDataWrite_Falcon(buffer, sectorCount);
        }
        
        if(res) {                                       // if failed, quit
            return -1;
        }
    }
    
    //---------
    // read status
    res = getStatusByte();
    return res;
}

BYTE selscsi_falcon(void)
{
    BYTE res;
    DWORD timeOutTime = setscstmout();      // set up a short timeout

    scsi_setReg_Falcon(REG_ICR, 0x0d);      // assert BUSY, SEL and data bus

    BYTE selId  = (1 << deviceID);          // convert number of device to bit 
    scsi_setReg_Falcon(REG_ODR, selId);     // output SCSI ID which we want to select
    
    scsi_setReg_Falcon(REG_TCR, 0);         // I/O=0, MSG=0, C/D=0 (wichtig!)
    scsi_setReg_Falcon(REG_ICR, 0x0d);      // assert BUSY, SEL and data bus
    
    scsi_clrBit_Falcon(REG_MR, MR_ARBIT);   // finish arbitration

    scsi_setReg_Falcon(REG_CR,  0);         // clear BSY, set ATN
    scsi_setReg_Falcon(REG_ICR, 0x05);      // clear BSY

    timeOutTime = setscstmout();            // set up a short timeout
    
    while(1) {                              // wait for busy bit to appear
        BYTE icr = scsi_getReg_Falcon(REG_CR);
        
        if(icr & ICR_BUSY) {                // if bit set, good
            res = 0;
            break;
        }
        
        DWORD now = *HZ_200;
        if(now >= timeOutTime) {            // if time out, fail
            res = -1;
            break;
        }        
    }
    
    scsi_setReg_Falcon(REG_ICR, 0);             // clear SEL and data bus assertion
    return res;
}

WORD getStatusByte(void)
{
    BYTE status, __attribute__((unused)) msg;
    
   	scsi_setReg_Falcon(REG_TCR, TCR_PHASE_STATUS);      // STATUS IN phase
	scsi_getReg_Falcon(REG_REI);                        // clear potential interrupt

    status = pio_read();                                // read status byte
    
   	scsi_setReg_Falcon(REG_TCR, TCR_PHASE_MESSAGE_IN);  // MESSAGE IN phase
	scsi_getReg_Falcon(REG_REI);                        // clear potential interrupt

    msg = pio_read();                                   // read message byte
    
    return status;
}    

BYTE pio_write(BYTE val)
{
    DWORD timeOutTime = setscstmout();              // set up time-out for REQ and ACK
    BYTE res;
    
    res = w4req_Falcon(timeOutTime);                // wait for REQ
    if(res) {                                       // if timed-out, fail
        return -1;
    }

    scsi_setReg_Falcon(REG_ODR, val);               // write cmd byte to ODR

    res = doack_Falcon(timeOutTime);                // assert ACK
    return res;
}

WORD pio_read(void)
{
    DWORD timeOutTime = setscstmout();              // set up time-out for REQ and ACK
    BYTE res, val;
    
    res = w4req_Falcon(timeOutTime);                // wait for REQ
    if(res) {                                       // if timed-out, fail
        return -1;
    }

    val = scsi_getReg_Falcon(REG_ODR);              // read byte from bus
    
    res = doack_Falcon(timeOutTime);                // assert ACK
    if(res) {
        return -1;
    }
    
    return val;
}

BYTE w4req_Falcon(DWORD timeOutTime) 
{
    while(1) {                      // wait for REQ
        BYTE icr = scsi_getReg_Falcon(REG_CR);
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
BYTE doack_Falcon(DWORD timeOutTime)
{
    BYTE icr    = scsi_getReg_Falcon(REG_ICR);
    icr         = icr | 0x11;           // assert ACK (and data bus)
    scsi_setReg_Falcon(REG_ICR, icr);

    BYTE res;
    
    while(1) {
        BYTE icr = scsi_getReg_Falcon(REG_ICR);
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

    scsi_clrBit_Falcon(REG_ICR, ICR_ACK);   // clear ACK
    return res;
}

BYTE getCurrentScsiPhase(void)
{
    BYTE val = scsi_getReg_Falcon(REG_CR);
    
    val = (val >> 2) & 0x07;                // get only bits 4,3,2 (MSG, CD, IO)
    return val;
}

BYTE dmaDataRead_Falcon(BYTE *buffer, WORD sectorCount)
{
    scsi_setReg_Falcon(REG_ICR, 0);                    // data bus as input
    scsi_setReg_Falcon(REG_TCR, TCR_PHASE_DATA_IN);    // set DATA IN  phase

    scsi_getReg_Falcon(REG_REI);                       // clear interrupts by reading REG_REI

    scsi_setBit_Falcon(REG_MR, MR_DMA);                // DMA mode ON

    setDmaAddr_Falcon((DWORD) buffer);                  // set DMA adress

    *WDL = 0x190;                                       // toggle DMA chip for "receive"
    delay();
    *WDL = 0x090;
    delay();
    
    *WDC = sectorCount;                     // write sector count, instead of using 'bsr setacnt'

    while(1) {                              // wait till it's safe to access the DMA channel
        BYTE sr = *WDSR;
        if(sr & (1 << 3)) {
            break;
        }
    }
    delay();

    scsi_setReg_Falcon(REG_DIR, 0);         // start DMA receive
    *WDL = 0;
    
    BYTE res = wait_dma_cmpl(200);          // wait for DMA completetion
    if(res) {                               // failed?
        return -1;
    }
    
    res = scsi_getReg_Falcon(REG_DS);       // get DMA STATUS
    
    stopDmaFalcon();

    if(res & (BSR_PARIERR | BSR_BUSYERR)) { // parity error or busy? fail
        return -1;
    }
        
    return 0;
}

BYTE dmaDataWrite_Falcon(BYTE *buffer, WORD sectorCount)
{
    scsi_setReg_Falcon(REG_TCR, TCR_PHASE_DATA_OUT);   // set DATA OUT phase
    scsi_setReg_Falcon(REG_ICR, 1);        // data bus as output

    scsi_getReg_Falcon(REG_REI);           // clear interrupts by reading REG_REI

    scsi_setBit_Falcon(REG_MR, MR_DMA);    // DMA mode ON

    scsi_setReg_Falcon(REG_DS, 0);
    
    setDmaAddr_Falcon((DWORD) buffer);      // set DMA adress

    *WDL = 0x090;                           // toggle DMA chip for "send"
    delay();
    *WDL = 0x190;
    delay();
    
    *WDC = sectorCount;                     // write sector count, instead of using 'bsr setacnt'

    while(1) {                              // wait till it's safe to access the DMA channel
        BYTE sr = *WDSR;
        if(sr & (1 << 3)) {
            break;
        }
    }
    delay();

    *WDL = 0x18d;
    *WDC = 0;
    *WDL = 0x100;
    
    BYTE res = wait_dma_cmpl(200);      // wait for DMA completetion
    if(res) {                           // failed?
        return -1;
    }
    
    res = scsi_getReg_Falcon(REG_DS);   // get DMA STATUS
    
    stopDmaFalcon();

    if(res & BSR_BUSYERR) {             // parity error or busy? fail
        return -1;
    }
        
    return 0;
}

void delay(void)
{
    (void) *MFP2;
    (void) *MFP2;
    (void) *MFP2;
}

void stopDmaFalcon(void)
{
    scsi_getReg_Falcon(REG_REI);           // reset ints by reading register
    scsi_clrBit_Falcon(REG_MR, MR_DMA);    // DMA mode off
    scsi_setReg_Falcon(REG_ICR, 0);        
    
    clearCache030();
}

void setDmaAddr_Falcon(DWORD addr)
{
    *falconDmaAddrLo    = (BYTE) (addr      );
    *falconDmaAddrMid   = (BYTE) (addr >>  8);
    *falconDmaAddrHi    = (BYTE) (addr >> 16);
}

void scsi_setReg_Falcon(int whichReg, DWORD value)
{
    BYTE which = 0;

    switch(whichReg) {
        case REG_DB :   which = SPCSD; break;   // for REG_DB  and REG_ODR
        case REG_ICR:   which = SPICR; break;
        case REG_MR :   which = SPMR2; break;
        case REG_TCR:   which = SPTCR; break;
        case REG_CR :   which = SPCSB; break;   // for REG_CR  and REG_ISR
        case REG_DS :   which = SPBSR; break;   // for REG_DSR and REG_DS
        case REG_DTR:   which = SPIDR; break;   // for REG_DTR and REG_IDR
        case REG_DIR:   which = SPRPI; break;   // for REG_DIR and REG_REI
        default     :   return;                 // fail, not found
    }
    
    *WDL    = which;    // select reg by writing to WDL
    *WDC    = value;    // write reg value by writing to WDC
}

BYTE scsi_getReg_Falcon(int whichReg)
{
    BYTE which = 0;
   
    switch(whichReg) {
        case REG_DB :   which = SPCSD; break;   // for REG_DB  and REG_ODR
        case REG_ICR:   which = SPICR; break;
        case REG_MR :   which = SPMR2; break;
        case REG_TCR:   which = SPTCR; break;
        case REG_CR :   which = SPCSB; break;   // for REG_CR  and REG_ISR
        case REG_DS :   which = SPBSR; break;   // for REG_DSR and REG_DS
        case REG_DTR:   which = SPIDR; break;   // for REG_DTR and REG_IDR
        case REG_DIR:   which = SPRPI; break;   // for REG_DIR and REG_REI
        default     :   return 0;               // fail, not found
    }
        
    BYTE val;
    *WDL    = which;    // select reg by writing to WDL
    val     = *WDC;     // read reg value by reading from WDC

    return val;
}

void scsi_setBit_Falcon(int whichReg, DWORD bitMask)
{
    DWORD val;
    val = scsi_getReg_Falcon(whichReg); // read
    val = val | bitMask;                // modify (set bits)
    scsi_setReg_Falcon(whichReg, val);  // write
}

void scsi_clrBit_Falcon(int whichReg, DWORD bitMask)
{
    DWORD val;
    DWORD invMask = ~bitMask;
    
    val = scsi_getReg_Falcon(whichReg); // read
    val = val & invMask;                // modify (clear bits)
    scsi_setReg_Falcon(whichReg, val);  // write
}
