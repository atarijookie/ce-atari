#include "hdd_if.h"
#include "scsi.h"

void delay(void);
void stopDmaFalcon(void);

static BYTE selscsi_falcon(BYTE scsiId);
static WORD getStatusByte(void);
static BYTE pio_write(BYTE val);
static WORD pio_read(void);
static BYTE w4req_Falcon(void); 
static BYTE doack_Falcon(void);

#define USE_DMA

#ifdef USE_DMA
static BYTE dmaDataWrite_Falcon(BYTE *buffer, WORD sectorCount);
static BYTE dmaDataRead_Falcon (BYTE *buffer, WORD sectorCount);
#else 
extern WORD pioDataTransfer(BYTE readNotWrite, BYTE *bfr, DWORD byteCount);
#endif

void logMsg(char *logMsg);

static void setDmaAddr_Falcon(DWORD addr);

void  scsi_setReg_Falcon(int whichReg, DWORD value);
DWORD scsi_getReg_Falcon(int whichReg);

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
    
    if(scsiId == 0) {                   // Trying to access SCSI ID 0 on Falcon? Fail, this is reserved for SCSI adapter
        return -1;
    }
    
    //---------
    falconCmdTimeOut = setscstmout();                   // set up a short timeout

    // select device
    BYTE res = selscsi_falcon(scsiId);                  // do device selection
    if(res) {
        return -1;
    }

    //---------
    // send command
    (*hdIf.pSetReg)(REG_TCR, TCR_PHASE_CMD);                 // set COMMAND PHASE (assert C/D)
    (*hdIf.pSetReg)(REG_ICR, 1);                             // data bus as output

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
        if(readNotWrite) {                                  // read
            (*hdIf.pSetReg)(REG_ICR, 0);                         // deassert the data bus
            (*hdIf.pSetReg)(REG_TCR, TCR_PHASE_DATA_IN);         // set DATA IN  phase
        } else {                                            // write
            (*hdIf.pSetReg)(REG_ICR, ICR_DBUS);                  // assert data bus
            (*hdIf.pSetReg)(REG_TCR, TCR_PHASE_DATA_OUT);        // set DATA OUT phase
        }

        res = (*hdIf.pGetReg)(REG_REI);             // clear potential interrupt

#ifdef USE_DMA
        res = dmaDataTx_do_Falcon(readNotWrite);
#else
        DWORD byteCount = sectorCount * 512;
        res = pioDataTransfer(readNotWrite, buffer, byteCount);
#endif

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

    (*hdIf.pSetReg)(REG_ICR, 0x0d);      // assert BUSY, SEL and data bus

    BYTE selId  = (1 << scsiId);            // convert number of device to bit 
    (*hdIf.pSetReg)(REG_ODR, selId);     // output SCSI ID which we want to select
    
    (*hdIf.pSetReg)(REG_TCR, 0);         // I/O=0, MSG=0, C/D=0 (wichtig!)
    (*hdIf.pSetReg)(REG_ICR, 0x0d);      // assert BUSY, SEL and data bus
    
    scsi_clrBit(REG_MR, MR_ARBIT);   // finish arbitration

    (*hdIf.pSetReg)(REG_CR,  0);         // clear BSY, set ATN
    (*hdIf.pSetReg)(REG_ICR, 0x05);      // clear BSY

    while(1) {                              // wait for busy bit to appear
        BYTE icr = (*hdIf.pGetReg)(REG_CR);
        
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
    
    (*hdIf.pSetReg)(REG_ICR, 0);         // clear SEL and data bus assertion
    return res;
}

WORD getStatusByte(void)
{
    BYTE status, __attribute__((unused)) msg;
    
   	(*hdIf.pSetReg)(REG_TCR, TCR_PHASE_STATUS);      // STATUS IN phase
	(*hdIf.pGetReg)(REG_REI);                        // clear potential interrupt

    status = pio_read();                                // read status byte
    
   	(*hdIf.pSetReg)(REG_TCR, TCR_PHASE_MESSAGE_IN);  // MESSAGE IN phase
	(*hdIf.pGetReg)(REG_REI);                        // clear potential interrupt

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

    (*hdIf.pSetReg)(REG_ODR, val);               // write cmd byte to ODR

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

    val = (*hdIf.pGetReg)(REG_ODR);              // read byte from bus
    
    res = doack_Falcon();                           // assert ACK
    if(res) {
        return -1;
    }
    
    return val;
}

BYTE w4req_Falcon(void) 
{
    while(1) {                          // wait for REQ
        BYTE icr = (*hdIf.pGetReg)(REG_CR);
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
    scsi_setBit(REG_ICR, ICR_ACK | ICR_DBUS);    // assert ACK (and data bus)

    BYTE res;
    
    while(1) {
        BYTE icr = (*hdIf.pGetReg)(REG_ICR);
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

    scsi_clrBit(REG_ICR, ICR_ACK);   // clear ACK
    return res;
}

BYTE dmaDataRead_Falcon(BYTE *buffer, WORD sectorCount)
{
logMsg("dmaDataRead_Falcon - A\n\r");

    (*hdIf.pSetReg)(REG_ICR, 0);                    // data bus as input
    (*hdIf.pSetReg)(REG_TCR, TCR_PHASE_DATA_IN);    // set DATA IN  phase

    (*hdIf.pGetReg)(REG_REI);                       // clear interrupts by reading REG_REI

    scsi_setBit(REG_MR, MR_DMA);                // DMA mode ON

logMsg("dmaDataRead_Falcon - B\n\r");

    setDmaAddr_Falcon((DWORD) buffer);                  // set DMA adress

logMsg("dmaDataRead_Falcon - C\n\r");
    
    *WDL = 0x190;                                       // toggle DMA chip for "receive"
    delay();
    *WDL = 0x090;
    delay();
    
    *WDC = sectorCount;                     // write sector count, instead of using 'bsr setacnt'

logMsg("dmaDataRead_Falcon - D\n\r");
    
    while(1) {                              // wait till it's safe to access the DMA channel
        BYTE sr = *WDSR;
        if(sr & (1 << 3)) {
            break;
        }
    }
    delay();

    (*hdIf.pSetReg)(REG_DIR, 0);         // start DMA receive
    *WDL = 0;

logMsg("dmaDataRead_Falcon - E\n\r");
    
    BYTE res = wait_dma_cmpl(200);          // wait for DMA completetion
    if(res) {                               // failed?
        return -1;
    }
    
logMsg("dmaDataRead_Falcon - F\n\r");
    
    res = (*hdIf.pGetReg)(REG_SDS);      // get DMA STATUS
    
    stopDmaFalcon();

logMsg("dmaDataRead_Falcon - G\n\r");
    
    if(res & (BSR_PARIERR | BSR_BUSYERR)) { // parity error or busy? fail
        return -1;
    }
        
    return 0;
}

BYTE dmaDataWrite_Falcon(BYTE *buffer, WORD sectorCount)
{
    (*hdIf.pSetReg)(REG_TCR, TCR_PHASE_DATA_OUT);   // set DATA OUT phase
    (*hdIf.pSetReg)(REG_ICR, 1);        // data bus as output

    (*hdIf.pGetReg)(REG_REI);           // clear interrupts by reading REG_REI

    scsi_setBit(REG_MR, MR_DMA);    // DMA mode ON

    (*hdIf.pSetReg)(REG_SDS, 0);
    
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
    
    res = (*hdIf.pGetReg)(REG_SDS);   // get DMA STATUS
    
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
    (*hdIf.pGetReg)(REG_REI);           // reset ints by reading register
    scsi_clrBit(REG_MR, MR_DMA);    // DMA mode off
    (*hdIf.pSetReg)(REG_ICR, 0);        
    
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
        default     :   logMsg("setReg - default!!!\n\r");  return;     // fail, not found
    }
    
    *WDL    = which;    // select reg by writing to WDL
    *WDC    = value;    // write reg value by writing to WDC
}

DWORD scsi_getReg_Falcon(int whichReg)
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
        default     :   logMsg("getReg - default!!!\n\r");  return 0;     // fail, not found
    }
        
    BYTE val;
    *WDL    = which;    // select reg by writing to WDL
    val     = *WDC;     // read reg value by reading from WDC

    return val;
}

void dmaDataTx_prepare_Falcon(BYTE readNotWrite, BYTE *buffer, DWORD dataByteCount)
{
    // set DMA pointer to buffer address
    setDmaAddr_Falcon((DWORD) buffer);

    WORD wdl1, wdl2;

    if(readNotWrite) {                      // on read
        wdl1 = 0x190;
        wdl2 = 0x090;
    } else {                                // for write
        wdl1 = 0x090;
        wdl2 = 0x190;
    }
    
    // set DMA count
    *WDL = wdl1;                           // toggle DMA chip 
    delay();
    *WDL = wdl2;
    delay();
    
    *WDC = (dataByteCount >> 9);           // write sector count (not byte count)
    
    while(1) {                              // wait till it's safe to access the DMA channel
        BYTE sr = *WDSR;
        if(sr & (1 << 3)) {
            break;
        }
    }
    delay();
}

BYTE dmaDataTx_do_Falcon(BYTE readNotWrite)
{
    // Set up the DMA for data transfer
    (*hdIf.pSetReg)(REG_MR, MR_DMA);                // enable DMA mode
    
    if(readNotWrite) {                              // on read
        (*hdIf.pSetReg)(REG_DIR, 0);                // start the DMA receive
        
        *WDL = 0;
    } else {                                        // on write
        (*hdIf.pSetReg)(REG_SDS, 0);                // start the DMA send -- WrSCSI  #0,SDS

        *WDL = 0x18d;
        *WDC = 0;
        *WDL = 0x100;
    }
    
    BYTE res = wait_dma_cmpl(200);              // wait for DMA completetion
    if(res) {                                   // failed?
        logMsg(" dmaDataTansfer() failed - w4int() timeout\r\n");
        return -1;
    }
    
    res = (*hdIf.pGetReg)(REG_SDS);             // get DMA STATUS
    
    stopDmaFalcon();

    if(res & (BSR_PARIERR | BSR_BUSYERR)) {     // parity error or busy? fail
        return -1;
    }

    if(readNotWrite) {                          // if read, clear cache
    	clearCache030();
    }
    
    return 0;
}

