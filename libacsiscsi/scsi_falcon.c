#include "hdd_if.h"
#include "scsi.h"
#include "acsi.h"

void delay(void);
void stopDmaFalcon(void);

extern uint16_t pioDataTransfer(uint8_t readNotWrite, uint8_t *bfr, uint32_t byteCount);

void logMsg(char *logMsg);

static void setDmaAddr_Falcon(uint32_t addr);

void  scsi_setReg_Falcon(int whichReg, uint32_t value);
uint32_t scsi_getReg_Falcon(int whichReg);

void clearCache030(void);
void delay(void);

extern uint32_t _cmdTimeOut;                           // timeout time for scsi_cmd() from start to end

uint8_t w4int(void);

void stopDmaFalcon(void)
{
    (*hdIf.pGetReg)(REG_ResetParityInterrupts);           // reset ints by reading register
    scsi_clrBit(REG_Mode, MR_DMA);        // DMA mode off
    (*hdIf.pSetReg)(REG_InitiatorCommand, 0);

    clearCache030();
}

void setDmaAddr_Falcon(uint32_t addr)
{
    *falconDmaAddrLo    = (uint8_t) (addr      );
    *falconDmaAddrMid   = (uint8_t) (addr >>  8);
    *falconDmaAddrHi    = (uint8_t) (addr >> 16);
}

void scsi_setReg_Falcon(int whichReg, uint32_t value)
{
    uint8_t which = 0;

    switch(whichReg) {
        case REG_CurrentScsiData :   which = SPCSD; break;   // for REG_CurrentScsiData  and REG_OutputData
        case REG_InitiatorCommand:   which = SPICR; break;
        case REG_Mode :   which = SPMR2; break;
        case REG_TargetCommand:   which = SPTCR; break;
        case REG_CurrentScsiBusStatus :   which = SPCSB; break;   // for REG_CurrentScsiBusStatus  and REG_SelectEnable
        case REG_StartDmaSend:   which = SPBSR; break;   // for REG_BusAndStatus and REG_DS
        case REG_StartDmaTargetReceive:   which = SPIDR; break;   // for REG_StartDmaTargetReceive and REG_InputData
        case REG_StartDmaInitiatorReceive:   which = SPRPI; break;   // for REG_StartDmaInitiatorReceive and REG_ResetParityInterrupts
        default     :   logMsg("setReg - default!!!\n\r");  return;     // fail, not found
    }

    *WDL    = which;    // select reg by writing to WDL
    *WDC    = value;    // write reg value by writing to WDC
}

uint32_t scsi_getReg_Falcon(int whichReg)
{
    uint8_t which = 0;

    switch(whichReg) {
        case REG_CurrentScsiData :   which = SPCSD; break;   // for REG_CurrentScsiData  and REG_OutputData
        case REG_InitiatorCommand:   which = SPICR; break;
        case REG_Mode :   which = SPMR2; break;
        case REG_TargetCommand:   which = SPTCR; break;
        case REG_CurrentScsiBusStatus :   which = SPCSB; break;   // for REG_CurrentScsiBusStatus  and REG_SelectEnable
        case REG_StartDmaSend:   which = SPBSR; break;   // for REG_BusAndStatus and REG_DS
        case REG_StartDmaTargetReceive:   which = SPIDR; break;   // for REG_StartDmaTargetReceive and REG_InputData
        case REG_StartDmaInitiatorReceive:   which = SPRPI; break;   // for REG_StartDmaInitiatorReceive and REG_ResetParityInterrupts
        default     :   logMsg("getReg - default!!!\n\r");  return 0;     // fail, not found
    }

    uint8_t val;
    *WDL    = which;    // select reg by writing to WDL
    val     = *WDC;     // read reg value by reading from WDC

    return val;
}

uint8_t dmaDataTx_prepare_Falcon(uint8_t readNotWrite, uint8_t *buffer, uint32_t dataByteCount)
{

    return 0;
}

uint8_t dmaDataTx_do_Falcon(uint8_t readNotWrite, uint8_t *buffer, uint32_t dataByteCount)
{
    // Set up the DMA for data transfer
    (*hdIf.pSetReg)(REG_Mode, MR_DMA);                // enable DMA mode

    if(!readNotWrite) {                             // on write
        (*hdIf.pSetReg)(REG_StartDmaSend, 0);                // start the DMA send -- WrSCSI  #0,SDS
    }

    // set DMA pointer to buffer address
    setDmaAddr_Falcon((uint32_t) buffer);

    uint16_t wdl1, wdl2;
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
        uint8_t sr = *WDSR;
        if((sr & (1 << 3)) == 0) {                  // TODO: check if this is OK
            break;
        }

        uint32_t now = *HZ_200;
        if(now >= _cmdTimeOut) {                    // if time out, fail
            return -1;
        }
    }
    delay();

    if(readNotWrite) {                              // on read
        (*hdIf.pSetReg)(REG_StartDmaInitiatorReceive, 0);                // start the DMA receive

        *WDL = 0;                                   // turn on DMA read
    } else {                                        // on write
        *WDL = 0x18D;
        *WDC = 0;                   // TODO: similar to (*hdIf.pSetReg)(REG_StartDmaSend, 0);

        *WDL = 0x100;               // DMA_WR, DMA enable
    }

    uint32_t now = *HZ_200;
    uint32_t ticksRemaining = _cmdTimeOut - now;   // get how many ticks are remaining after previous operations

    uint8_t res = wait_dma_cmpl(ticksRemaining);   // wait for DMA completetion
    if(res) {                                   // failed?
        stopDmaFalcon();

        logMsg(" dmaDataTansfer() failed - wait_dma_cmpl() timeout\r\n");
        return -1;
    }

    res = (*hdIf.pGetReg)(REG_StartDmaSend);                 // get DMA STATUS

    stopDmaFalcon();

    if(res & (BSR_PARIERR | BSR_BUSYERR)) {         // parity error or busy? fail
        return -1;
    }

    if(readNotWrite) {                              // if read, clear cache
    	clearCache030();
    }

    return 0;
}
