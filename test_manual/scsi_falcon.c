#include "scsi.h"

void delay(void);
void stopDmaFalcon(void);

static BYTE selscsi_falcon(BYTE scsiId);
static BYTE dmaDataWrite_Falcon(BYTE *buffer, WORD sectorCount);
static BYTE dmaDataRead_Falcon(BYTE *buffer, WORD sectorCount);
static WORD getStatusByte(void);
static BYTE pio_write(BYTE val);
static WORD pio_read(void);
static BYTE w4req_Falcon(void); 
static BYTE doack_Falcon(void);

static void setDmaAddr_Falcon(DWORD addr);

static void scsi_setReg_Falcon(int whichReg, DWORD value);
static BYTE scsi_getReg_Falcon(int whichReg);
static void scsi_setBit_Falcon(int whichReg, DWORD bitMask);
static void scsi_clrBit_Falcon(int whichReg, DWORD bitMask);

DWORD setscstmout(void);
BYTE  wait_dma_cmpl(DWORD t_ticks);

void clearCache030(void);
static DWORD falconCmdTimeOut;

BYTE scsi_cmd_Falcon(BYTE readNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount)
{
    //------------
    // first we start by extracting ID and fixing the cmd[] array because there's different format of this for ACSI and SCSI
    BYTE scsiId = (cmd[0] >> 5);        // get only drive ID bits

    cmd[0] = cmd[0] & 0x1f;             // remove possible drive ID bits
    if((cmd[0] & 0x1f) == 0x1f) {       // if it's ICD format of command, skip the 0th byte
        cmd++;
        cmdLength--;
    }
    
    //---------
    falconCmdTimeOut = setscstmout();       // set up a short timeout
    
    // select device
    BYTE res = selscsi_falcon(scsiId);                  // do device selection
    if(res) {
        return -1;
    }
    
    //---------
    // send command
    scsi_setReg_Falcon(REG_TCR, TCR_PHASE_CMD);         // set COMMAND PHASE (assert C/D)
    scsi_setReg_Falcon(REG_ICR, 1);                     // data bus as output

    int i;
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

BYTE selscsi_falcon(BYTE scsiId)
{
    BYTE res;

    scsi_setReg_Falcon(REG_ICR, 0x0d);      // assert BUSY, SEL and data bus

    BYTE selId  = (1 << scsiId);            // convert number of device to bit 
    scsi_setReg_Falcon(REG_ODR, selId);     // output SCSI ID which we want to select
    
    scsi_setReg_Falcon(REG_TCR, 0);         // I/O=0, MSG=0, C/D=0 (wichtig!)
    scsi_setReg_Falcon(REG_ICR, 0x0d);      // assert BUSY, SEL and data bus
    
    scsi_clrBit_Falcon(REG_MR, MR_ARBIT);   // finish arbitration

    scsi_setReg_Falcon(REG_CR,  0);         // clear BSY, set ATN
    scsi_setReg_Falcon(REG_ICR, 0x05);      // clear BSY

    while(1) {                              // wait for busy bit to appear
        BYTE icr = scsi_getReg_Falcon(REG_CR);
        
        if(icr & ICR_BUSY) {                // if bit set, good
            res = 0;
            break;
        }
        
        DWORD now = *HZ_200;
        if(now >= falconCmdTimeOut) {       // if time out, fail
            res = -1;
            break;
        }        
    }
    
    scsi_setReg_Falcon(REG_ICR, 0);         // clear SEL and data bus assertion
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
    BYTE res;
    
    res = w4req_Falcon();                           // wait for REQ
    if(res) {                                       // if timed-out, fail
        return -1;
    }

    scsi_setReg_Falcon(REG_ODR, val);               // write cmd byte to ODR

    res = doack_Falcon();                           // assert ACK
    return res;
}

WORD pio_read(void)
{
    BYTE res, val;
    
    res = w4req_Falcon();                           // wait for REQ
    if(res) {                                       // if timed-out, fail
        return -1;
    }

    val = scsi_getReg_Falcon(REG_ODR);              // read byte from bus
    
    res = doack_Falcon();                           // assert ACK
    if(res) {
        return -1;
    }
    
    return val;
}

BYTE w4req_Falcon(void) 
{
    while(1) {                          // wait for REQ
        BYTE icr = scsi_getReg_Falcon(REG_CR);
        if(icr & ICR_REQ) {             // if REQ appeared, good
            return 0;
        }
        
        DWORD now = *HZ_200;
        if(now >= falconCmdTimeOut) {   // if time out, fail
            break;
        }
    }
    
    return -1;                          // time out
}

// doack() - assert ACK
BYTE doack_Falcon(void)
{
    scsi_setBit_Falcon(REG_ICR, ICR_ACK | ICR_DBUS);    // assert ACK (and data bus)

    BYTE res;
    
    while(1) {
        BYTE icr = scsi_getReg_Falcon(REG_ICR);
        if((icr & ICR_REQ) == 0) {          // if REQ gone, good
            res = 0;
            break;
        }
        
        DWORD now = *HZ_200;
        if(now >= falconCmdTimeOut) {       // if time out, fail
            res = -1;
            break;
        }
    }

    scsi_clrBit_Falcon(REG_ICR, ICR_ACK);   // clear ACK
    return res;
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
    
    res = scsi_getReg_Falcon(REG_SDS);      // get DMA STATUS
    
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

    scsi_setReg_Falcon(REG_SDS, 0);
    
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
    
    res = scsi_getReg_Falcon(REG_SDS);   // get DMA STATUS
    
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
        case REG_SDS:   which = SPBSR; break;   // for REG_DSR and REG_DS
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
        case REG_SDS:   which = SPBSR; break;   // for REG_DSR and REG_DS
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
