#include "hdd_if.h"
#include "scsi.h"
#include "acsi.h"

void delay(void);
void stopDmaFalcon(void);

extern WORD pioDataTransfer(BYTE readNotWrite, BYTE *bfr, DWORD byteCount);

void logMsg(char *logMsg);

static void setDmaAddr_Falcon(DWORD addr);

void  scsi_setReg_Falcon(int whichReg, DWORD value);
DWORD scsi_getReg_Falcon(int whichReg);

DWORD setscstmout(void);

void clearCache030(void);
void delay(void);

extern DWORD _cmdTimeOut;                           // timeout time for scsi_cmd() from start to end

BYTE w4int(void);

void stopDmaFalcon(void)
{
    (*hdIf.pGetReg)(REG_REI);           // reset ints by reading register
    scsi_clrBit(REG_MR, MR_DMA);        // DMA mode off
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

BYTE dmaDataTx_prepare_Falcon(BYTE readNotWrite, BYTE *buffer, DWORD dataByteCount)
{

    return 0;
}

BYTE dmaDataTx_do_Falcon(BYTE readNotWrite, BYTE *buffer, DWORD dataByteCount)
{
    // Set up the DMA for data transfer
    (*hdIf.pSetReg)(REG_MR, MR_DMA);                // enable DMA mode

    if(!readNotWrite) {                             // on write
        (*hdIf.pSetReg)(REG_SDS, 0);                // start the DMA send -- WrSCSI  #0,SDS
    }

    // set DMA pointer to buffer address
    setDmaAddr_Falcon((DWORD) buffer);

    WORD wdl1, wdl2;
    if(readNotWrite) {                              // on read
        wdl1 = 0x190;
        wdl2 = 0x090;
    } else {                                        // for write
        wdl1 = 0x090;
        wdl2 = 0x190;
    }

    // set DMA count
    *WDL = wdl1;                                    // toggle DMA chip
    delay();
    *WDL = wdl2;
    delay();

    *WDC = (dataByteCount >> 9);                    // write sector count (not byte count)

    while(1) {                                      // wait till it's safe to access the DMA channel
        BYTE sr = *WDSR;
        if((sr & (1 << 3)) == 0) {         // ??? NEMAM TOTO OPACNE ???
            break;
        }

        DWORD now = *HZ_200;
        if(now >= _cmdTimeOut) {                    // if time out, fail
            return -1;
        }
    }
    delay();

    if(readNotWrite) {                              // on read
        (*hdIf.pSetReg)(REG_DIR, 0);                // start the DMA receive

        *WDL = 0;                                   // turn on DMA read
    } else {                                        // on write
        *WDL = 0x18D;
        *WDC = 0;                   // ??? Podobne ako (*hdIf.pSetReg)(REG_SDS, 0);

        *WDL = 0x100;               // DMA_WR, DMA enable
    }

    BYTE res = wait_dma_cmpl(200);                             // wait for DMA completetion
    if(res) {                                       // failed?
        stopDmaFalcon();

        logMsg(" dmaDataTansfer() failed - wait_dma_cmpl() timeout\r\n");
        return -1;
    }

    res = (*hdIf.pGetReg)(REG_SDS);                 // get DMA STATUS

    stopDmaFalcon();

    if(res & (BSR_PARIERR | BSR_BUSYERR)) {         // parity error or busy? fail
        return -1;
    }

    if(readNotWrite) {                              // if read, clear cache
    	clearCache030();
    }

    return 0;
}
