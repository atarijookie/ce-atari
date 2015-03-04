#include "scsi.h"
#include "find_ce.h"

void delay(void);
void stopDmaFalcon(void);

static BYTE dmaDataWrite_Falcon(BYTE *buffer, WORD sectorCount);
static BYTE dmaDataRead_Falcon(BYTE *buffer, WORD sectorCount);
static BYTE sendCmdByte(BYTE cmd);
static BYTE getCurrentScsiPhase(void);
static BYTE getStatusByte(void);
static void scsi_setReg_Falcon(int whichReg, DWORD value);
static BYTE scsi_getReg_Falcon(int whichReg);
static void scsi_setBit_Falcon(int whichReg, DWORD bitMask);
static void scsi_clrBit_Falcon(int whichReg, DWORD bitMask);
static void setDmaAddr_Falcon(DWORD addr);

DWORD setscstmout(void);
BYTE  w4req(DWORD timeOutTime);
BYTE  wait_dma_cmpl(DWORD t_ticks);

void clearCache(void);

BYTE scsi_cmd_falcon(BYTE readNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount)
{
    int i;
    BYTE res;
    
    for(i=0; i<cmdLength; i++) {        // try to send all cmd bytes
        res = sendCmdByte(cmd[i]);
        
        if(res) {                       // failed? quit
            return -1;
        }
        
        res = getCurrentScsiPhase();    // get current phase

        // TODO: if phase changed, stop sending cmd bytes
    }

    if(sectorCount > 0) {
        if(readNotWrite) {
            res = dmaDataRead_Falcon(buffer, sectorCount);
        } else {
            res = dmaDataWrite_Falcon(buffer, sectorCount);
        }
        
        if(res) {                       // if failed, quit
            return -1;
        }
    }
    
    res = getStatusByte();
    return res;
}

BYTE getStatusByte(void)
{
    BYTE res = scsi_getReg_Falcon(REG_DB);     // read status byte
    scsi_clrBit_Falcon(REG_ICR, ICR_ACK);      // clear ACK
    
    return res;
}

BYTE sendCmdByte(BYTE cmd)
{
    scsi_setReg_Falcon(REG_TCR, TCR_PHASE_CMD);    // set COMMAND PHASE (assert C/D)
    
    scsi_setReg_Falcon(REG_ODR, cmd);              // write cmd byte to ODR
    scsi_setReg_Falcon(REG_ICR, 1);                // data bus as output
    scsi_setBit_Falcon(REG_ICR, ICR_ACK);          // set ACK
    
    DWORD timeOutTime = setscstmout();              // set up time-out for REQ and ACK

    BYTE res = w4req(timeOutTime);                  // wait for REQ
    if(res) {                                       // if timed-out, fail
        return -1;
    }

    return 0;
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
    clearCache();
}

void clearCache(void)
{
/*
        tst.w   cacheflag               ; Cacheflag testen
        bmi.s   chk4cache              ; schon initialisiert?

clearit:
        beq.s   .rts                    ; kein Cache, dann weiter

        move.l  d0,-(sp)  
        move.w  cacheflag,d0
        cmp.w   #40,d0                  ; 40 oder h”her?
        bge.s   .clr040

        move    SR,-(SP)                ; Status merken
        ori     #$0700,SR               ; IRQs kurz aus
        DC.L    movecrd0                ; movec cacr,d0
        or.w    #$0808,D0               ; Daten- und Instruktionscache l”schen
        DC.L    moved0cr                ; movec d0,cacr
        move    (SP)+,SR                ; Status holen
        bra.s   .exit

.clr040:
        dc.w    $F4F8
        nop

.exit:
        move.l  (sp)+,d0  
.rts:
        rts   
*/        
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
