// based on AHDI 6.061 sources

#include "hdd_if.h"
#include "scsi.h"
#include "acsi.h"
#include "stdlib.h"

#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <support.h>

#include <stdint.h>
#include <stdio.h>

void logMsg(char *logMsg);
void logMsgProgress(uint32_t current, uint32_t total);

void scsi_reset(void);
//-----------------
// local function definitions
static uint8_t scsiSelectAndSendCmd(uint8_t readNotWrite, uint8_t scsiId, uint8_t *cmd, uint8_t cmdLength, uint8_t *dataAddr, uint32_t dataByteCount);
static uint8_t scsiSelection(uint8_t scsiId);
static uint16_t dataTransfer(uint8_t readNotWrite, uint8_t *bfr, uint32_t byteCount, uint8_t cmdLength);
static void scsiGetStatusAndMsgIn(void);
static uint8_t scsiAck(void);
       void scsiSetLongTimeout(void);
       uint8_t scsiWaitForReq(void);
       void scsiSetShortTimeout(void);

uint8_t PIO_read(void);
void PIO_write(uint8_t data);

#define USE_DMA

#ifdef USE_DMA
uint8_t  scsiWaitForInt(void);
void     setDmaAddr_TT(uint32_t addr);
uint32_t getDmaAddr_TT(void);
void     setDmaCnt_TT(uint32_t dataCount);
#else
uint16_t pioDataTransfer(uint8_t readNotWrite, uint8_t *bfr, uint32_t byteCount);
uint16_t pioDataTransfer_read(uint8_t *bfr, uint32_t byteCount);
uint16_t pioDataTransfer_write(uint8_t *bfr, uint32_t byteCount);
#endif

extern uint8_t machine;

uint32_t scsiGetRegTT(int whichReg);
void  scsiSetRegTT(int whichReg, uint32_t value);

uint8_t dmaDataTx_prepare_TT (uint8_t readNotWrite, uint8_t *buffer, uint32_t dataByteCount);
uint8_t dmaDataTx_do_TT      (uint8_t readNotWrite, uint8_t *buffer, uint32_t dataByteCount);
//-----------------

void clearCache030(void);

uint32_t _cmdTimeOut;                      // timeout time for scsi_cmd() from start to end
void scsiCmdTT(uint8_t readNotWrite, uint8_t *cmd, uint8_t cmdLength, uint8_t *buffer, uint16_t sectorCount)
{
    //--------
    // init result to fail codes
    hdIf.success        = FALSE;
    hdIf.statusByte     = ACSIERROR;
    hdIf.phaseChanged   = FALSE;

    //-------------
    // create local copy of cmd[]
    uint8_t tmpCmd[32];
    uint8_t tmpCmdLen = (cmdLength < 32) ? cmdLength : 32;
    memcpy(tmpCmd, cmd, tmpCmdLen);

    //------------
    // first we start by extracting ID and fixing the cmd[] array because there's different format of this for ACSI and SCSI
    uint8_t scsiId = (tmpCmd[0] >> 5);     // get only drive ID bits

    tmpCmd[0] = tmpCmd[0] & 0x1f;       // remove possible drive ID bits

    if((tmpCmd[0] & 0x1f) == 0x1f) {    // if it's ICD format of command, skip the 0th byte
        cmd = &tmpCmd[1];
        cmdLength--;
    } else {                            // not ICD command, start from 0th byte
        cmd = &tmpCmd[0];
    }

    if(scsiId == hdIf.scsiHostId) {     // Trying to access reserved SCSI ID? Fail... (skip)
        hdIf.success = FALSE;
        return;
    }

    //------------
    *FLOCK = 0xffff;                    // set FLOCK to disable FDC operations

    scsiSetShortTimeout();              // short timeout for command
    uint8_t res = scsiSelectAndSendCmd(readNotWrite, scsiId, cmd, cmdLength, buffer, sectorCount << 9);      // send command block

    if(res) {
        logMsg("scsiCmdTT failed on scsiSelectAndSendCmd() \r\n");
        *FLOCK = 0;                     // clear FLOCK to enable FDC operations

        hdIf.success = FALSE;
        return;
    }

    scsiSetLongTimeout();               // long timeout for data

    if(sectorCount != 0) {
        uint32_t byteCount = sectorCount << 9;
        uint16_t wres = dataTransfer(readNotWrite, buffer, byteCount, cmdLength);

        if(wres) {
            logMsg("scsiCmdTT failed on dataTransfer \r\n");
            *FLOCK = 0;                 // clear FLOCK to enable FDC operations

            hdIf.success = FALSE;
            return;
        }
    }

    scsiGetStatusAndMsgIn();                           // wait for status byte

    if(!hdIf.success) {
        logMsg("scsiCmdTT failed on scsiGetStatusAndMsgIn \r\n");
    }

    *FLOCK = 0;                         // clear FLOCK to enable FDC operations
}

uint16_t dataTransfer(uint8_t readNotWrite, uint8_t *bfr, uint32_t byteCount, uint8_t cmdLength)
{
    uint16_t res;

    if(readNotWrite) {                                  // read
        (*hdIf.pSetReg)(REG_InitiatorCommand, 0);                         // deassert the data bus
        (*hdIf.pSetReg)(REG_TargetCommand, TCR_PHASE_DATA_IN);         // set DATA IN  phase
    } else {                                            // write
        (*hdIf.pSetReg)(REG_InitiatorCommand, ICR_DBUS);                  // assert data bus
        (*hdIf.pSetReg)(REG_TargetCommand, TCR_PHASE_DATA_OUT);        // set DATA OUT phase
    }

    res = (*hdIf.pGetReg)(REG_ResetParityInterrupts);             // clear potential interrupt

#ifdef USE_DMA                    // if using DMA for data transfer
    res = (*hdIf.pDmaDataTx_do) (readNotWrite, bfr, byteCount);
#else                           // if using PIO for data transfer
    res = pioDataTransfer(readNotWrite, bfr, byteCount);
#endif

    return res;
}

#ifndef USE_DMA

uint16_t pioDataTransfer(uint8_t readNotWrite, uint8_t *bfr, uint32_t byteCount)
{
    uint16_t res;

    if(byteCount >= 0x3500) {
        (void) Cconws("!!! pioDataTransfer() will probably fail when transferring too much data, use DMA instead !!!\n\r");
    }

    if(readNotWrite) {          // read?
        res = pioDataTransfer_read(bfr, byteCount);
    } else {                    // write?
        res = pioDataTransfer_write(bfr, byteCount);
    }

    return res;                 // good
}

uint16_t pioDataTransfer_read(uint8_t *bfr, uint32_t byteCount)
{
    int i;
    uint8_t data;

    hdIf.phaseChanged = FALSE;

    for(i=0; i<byteCount; i++) {
        data = PIO_read();

        if(hdIf.phaseChanged) {         // phase changed? pretend no error
//            logMsg("pioDataTransfer_read() - phase changed while read\n\r");
            logMsgProgress(i, byteCount);
            return 0;
        }

        if(!hdIf.success) {           // other error? quit
            logMsg("pioDataTransfer_read() - other error\n\r");
            logMsgProgress(i, byteCount);
            return -1;
        }

        bfr[i] = data;
    }

    return 0;                       // good
}

uint16_t pioDataTransfer_write(uint8_t *bfr, uint32_t byteCount)
{
    int i;

    for(i=0; i<byteCount; i++) {
        PIO_write(bfr[i]);

        if(hdIf.phaseChanged) {         // phase changed? pretend no error
//            logMsg("pioDataTransfer_write - phase changed while write\n\r");
            logMsgProgress(i, byteCount);
            return 0;
        }

        if(!hdIf.success) {
            logMsg("pioDataTransfer_write - other error\n\r");
            logMsgProgress(i, byteCount);
            return -1;
        }
    }

    return 0;                       // good
}

#endif

// scsiSelectAndSendCmd() - set DMA pointer and count and send command block
uint8_t scsiSelectAndSendCmd(uint8_t readNotWrite, uint8_t scsiId, uint8_t *cmd, uint8_t cmdLength, uint8_t *dataAddr, uint32_t dataByteCount)
{
    uint8_t res;

    res = scsiSelection(scsiId);  // select required device

    if(res) {               // if failed, quit with failure
        logMsg("scsiSelectAndSendCmd() failed on SELECT\r\n");
        return -1;
    }

#ifdef USE_DMA
    res = (*hdIf.pDmaDataTx_prepare)(readNotWrite, dataAddr, dataByteCount);

    if(res) {
        logMsg("scsiSelectAndSendCmd() failed on DmaDataTx_prepare()\r\n");
        return -1;
    }
#endif

    (*hdIf.pSetReg)(REG_TargetCommand, TCR_PHASE_CMD);                // set COMMAND PHASE (assert C/D)
    (*hdIf.pSetReg)(REG_InitiatorCommand, ICR_DBUS);                     // assert data bus

    int i;

    for(i=0; i<cmdLength; i++) {                            // send all the cmd bytes using PIO
        PIO_write(cmd[i]);

        if(!hdIf.success) {                                 // if time out happened, fail
            logMsg("scsiSelectAndSendCmd() - CMD phase failed on PIO_write()\r\n");
            logMsgProgress(i, cmdLength);
            return -1;
        }
    }

    return 0;
}

// Selects the SCSI device with specified SCSI ID
uint8_t scsiSelection(uint8_t scsiId)
{
    uint8_t res;

    while(1) {                                              // STILL busy from last time?
        uint8_t icr = (*hdIf.pGetReg)(REG_CurrentScsiBusStatus);
        if((icr & ICR_BUSY) == 0) {                         // if not, it's available
            break;
        }

        uint32_t now = *HZ_200;
        if(now >= _cmdTimeOut) {                            // if time out, fail
            return -1;
        }
    }

    (*hdIf.pSetReg)(REG_TargetCommand, TCR_PHASE_DATA_OUT);           // data out phase
    (*hdIf.pSetReg)(REG_SelectEnable, 0);                            // no interrupt from selection
    (*hdIf.pSetReg)(REG_InitiatorCommand, ICR_BSY | ICR_SEL);            // assert BSY and SEL

    uint8_t selId  = (1 << scsiId);                            // convert number of device to bit
    (*hdIf.pSetReg)(REG_OutputData, selId);                        // set dest SCSI IDs

    (*hdIf.pSetReg)(REG_InitiatorCommand, ICR_BSY | ICR_SEL | ICR_DBUS); // assert BUSY, SEL and data bus
    scsi_clrBit(REG_Mode, MR_ARBIT);                          // clear arbitrate bit
    scsi_clrBit(REG_InitiatorCommand, ICR_BSY);                          // clear BUSY

    while(1) {                          // wait for busy bit to appear
        uint8_t icr = (*hdIf.pGetReg)(REG_CurrentScsiBusStatus);

        if(icr & ICR_BUSY) {            // if bit set, good
            res = 0;
            break;
        }

        uint32_t now = *HZ_200;
        if(now >= _cmdTimeOut) {                            // if time out, fail
            res = -1;
            break;
        }
    }

    (*hdIf.pSetReg)(REG_InitiatorCommand, 0);                            // clear SEL and data bus assertion
    return res;
}

void scsi_reset(void)
{
    (*hdIf.pSetReg)(REG_InitiatorCommand, ICR_RST);                      // assert RST

    _cmdTimeOut = *HZ_200 + 100;                            // wait 0.5 s

    uint32_t now;
    while(1) {
        now = *HZ_200;

        if(now >= _cmdTimeOut) {
            break;
        }
    }

    (*hdIf.pSetReg)(REG_InitiatorCommand, 0);                            // back to normal

    _cmdTimeOut = *HZ_200 + 100;                            // wait 0.5 s

    while(1) {
        now = *HZ_200;

        if(now >= _cmdTimeOut) {
            break;
        }
    }
}

// scsiWaitForInt - wait for interrupts from 5380 or DMAC during DMA transfer
// Comments:
//	When 5380 is interrupted, it indicates a change of data to status phase (i.e., DMA is done), or ...
//	When DMAC is interrupted, it indicates either DMA count is zero, or there is an internal bus error.
uint8_t scsiWaitForInt(void)
{
    uint8_t res;

    while(1) {
        res = *MFP2;
        if(res & GPIP2_NCR) {           // NCR 5380 interrupt?
            break;
        }

        if((res & GPIP2_DMA) == 0) {    // DMA interrupt?
            uint16_t wres = (*hdIf.pGetReg)(REG_DMACTL);    // get the DMAC status
            if(wres & 0x80) {           // check for bus err/ignore cntout ints
                return -1;
            }
        }

        uint32_t now = *HZ_200;
        if(now >= _cmdTimeOut) {            // time out? fail
            return -1;
        }
    }

    (*hdIf.pGetReg)(REG_ResetParityInterrupts);               // clear potential interrupt
    (*hdIf.pSetReg)(REG_DMACTL, DMADIS);    // disable DMA
    (*hdIf.pSetReg)(REG_Mode,  0);            // disable DMA mode
    (*hdIf.pSetReg)(REG_InitiatorCommand, 0);            // make sure data bus is not asserted

    return 0;
}

int scsiWaitForPhaseMatch(void)
{
    uint8_t phaseWant = ((*hdIf.pGetReg)(REG_TargetCommand)) & 0x07;

    while(1) {
        uint8_t phaseGot = (((*hdIf.pGetReg)(REG_CurrentScsiBusStatus)) >> 2) & 0x07;

        if(phaseGot == phaseWant) {     // phase match? good
            return 0;
        }

        uint32_t now = *HZ_200;
        if(now >= _cmdTimeOut) {        // if time out, fail
            logMsg("scsiWaitForPhaseMatch failed to wait for phase: ");
            logMsgHexByte(phaseWant);
            logMsg("\r\n");

            hdIf.success = FALSE;
            return -1;
        }
    }
}

// scsiGetStatusAndMsgIn - wait for status byte and message byte.
void scsiGetStatusAndMsgIn(void)
{
	(*hdIf.pSetReg)(REG_TargetCommand, TCR_PHASE_STATUS);     // STATUS IN phase
	(*hdIf.pGetReg)(REG_ResetParityInterrupts);                       // clear potential interrupt

    //-----------------
    // receive status byte
    uint8_t status = PIO_read();

    if(!hdIf.success) {                             // failed?
        logMsg("scsiGetStatusAndMsgIn failed on reading status byte \r\n");
        return;
    }

    //-----------------
    // receive message byte
	(*hdIf.pSetReg)(REG_TargetCommand, TCR_PHASE_MESSAGE_IN); // MESSAGE IN phase
	(*hdIf.pGetReg)(REG_ResetParityInterrupts);                       // clear potential interrupt

    (void) PIO_read();
    if(!hdIf.success) {
        logMsg("scsiGetStatusAndMsgIn failed on reading message in \r\n");
        return;
    }

    hdIf.success        = TRUE;                     // success!
    hdIf.statusByte     = status;                   // store status byte
}

uint8_t PIO_read(void)
{
    uint8_t res;
    (*hdIf.pSetReg)(REG_InitiatorCommand, 0);         // deassert data bus (disable data output)

    hdIf.phaseChanged   = FALSE;
    hdIf.success        = FALSE;

    res = scsiWaitForReq();                      // wait for status byte
    if(res) {                           // if timed-out, fail
        logMsg("PIO_read() - fail on scsiWaitForReq() \r\n");
        return 0;
    }

    res = (*hdIf.pGetReg)(REG_BusAndStatus);
    if((res & (1 << 3)) == 0) {         // PHASE MATCH bit from BUS AND STATUS REGISTER is low? SCSI phase changed
        uint8_t phaseWant = ((*hdIf.pGetReg)(REG_TargetCommand)) & 0x07;
        uint8_t phaseGot = (((*hdIf.pGetReg)(REG_CurrentScsiBusStatus)) >> 2) & 0x07;

        logMsg("PIO_read() - phase changed, fail - want: ");
        logMsgHexByte(phaseWant);
        logMsg(", got: ");
        logMsgHexByte(phaseGot);
        logMsg("\r\n");

        hdIf.phaseChanged   = TRUE;
        return 0;
    }

    uint8_t data = (*hdIf.pGetReg)(REG_CurrentScsiData); // get the status byte

    res = scsiAck();                      // signal that status byte is here
    if(res) {                           // if timed-out, fail
        logMsg("PIO_read() - fail on scsiAck() \r\n");
        return 0;
    }

    hdIf.success = TRUE;
    return data;
}

void PIO_write(uint8_t data)
{
    uint8_t res;

    hdIf.phaseChanged   = FALSE;
    hdIf.success        = FALSE;

    res = scsiWaitForReq();                      // wait for status byte
    if(res) {                           // if timed-out, fail
        logMsg("PIO_write() - fail on scsiWaitForReq() \r\n");
        return;
    }

    res = (*hdIf.pGetReg)(REG_BusAndStatus);
    if((res & (1 << 3)) == 0) {         // PHASE MATCH bit from BUS AND STATUS REGISTER is low? SCSI phase changed
//        logMsg("PIO_write() - phase change \r\n");

        hdIf.phaseChanged = FALSE;
        return;
    }

    (*hdIf.pSetReg)(REG_InitiatorCommand, ICR_DBUS);  // assert data bus (enable data output)
    (*hdIf.pSetReg)(REG_CurrentScsiData, data);

    res = scsiAck();                      // signal that status byte is here
    if(res) {                           // if timed-out, fail
        logMsg("PIO_write() - fail on scsiAck() \r\n");
        return;
    }

    hdIf.success = TRUE;
}

// scsiWaitForReq() - wait for REQ to come during hand shake of non-data bytes
uint8_t scsiWaitForReq(void)
{
    while(1) {                      // wait for REQ
        uint8_t icr = (*hdIf.pGetReg)(REG_CurrentScsiBusStatus);
        if(icr & ICR_REQ) {         // if REQ appeared, good
            return 0;
        }

        uint32_t now = *HZ_200;
        if(now >= _cmdTimeOut) {    // if time out, fail
            break;
        }
    }

    return -1;                      // time out
}

// scsiAck() - assert ACK
uint8_t scsiAck(void)
{
    scsi_setBit(REG_InitiatorCommand, ICR_ACK);   // assert ACK

    uint8_t res;

    while(1) {
        uint8_t icr = (*hdIf.pGetReg)(REG_InitiatorCommand);
        if((icr & ICR_REQ) == 0) {      // if REQ gone, good
            res = 0;
            break;
        }

        uint32_t now = *HZ_200;
        if(now >= _cmdTimeOut) {        // if time out, fail
            res = -1;
            break;
        }
    }

    scsi_clrBit(REG_InitiatorCommand, ICR_ACK);      // clear ACK
    return res;
}

// short timeout for command
void scsiSetShortTimeout(void)
{
    uint32_t now = *HZ_200;
    _cmdTimeOut = (now + SCSI_TIMEOUT_SHORT);
}

// long timeout for data
void scsiSetLongTimeout(void)
{
    uint32_t now = *HZ_200;
    _cmdTimeOut = (now + SCSI_TIMEOUT_LONG);
}

void setDmaAddr_TT(uint32_t addr)
{
    *bSDMAPTR_lo        = (uint8_t) (addr      );
    *bSDMAPTR_mid_lo    = (uint8_t) (addr >>  8);
    *bSDMAPTR_mid_hi    = (uint8_t) (addr >> 16);
    *bSDMAPTR_hi        = (uint8_t) (addr >> 24);
}

uint32_t getDmaAddr_TT(void)
{
    uint32_t  dmaPtr;
    dmaPtr = ((*bSDMAPTR_hi) << 24) | ((*bSDMAPTR_mid_hi) << 16) | ((*bSDMAPTR_mid_lo) << 8) | (*bSDMAPTR_lo);
    return dmaPtr;
}

void setDmaCnt_TT(uint32_t dataCount)
{
    *bSDMACNT_hi     = (uint8_t) (dataCount >> 24);
    *bSDMACNT_mid_hi = (uint8_t) (dataCount >> 16);
    *bSDMACNT_mid_lo = (uint8_t) (dataCount >>  8);
    *bSDMACNT_lo     = (uint8_t) (dataCount      );
}
//----------------------
// functions for SETING SCSI register
void scsiSetRegTT(int whichReg, uint32_t value)
{
    if(whichReg == REG_DMACTL) {
        *SDMACTL = value;
        return;
    }

    volatile uint8_t *pReg = (volatile uint8_t *) (0xFFFF8780 + whichReg);
    *pReg = (uint8_t) value;
}

//----------------------
// functions for GETTING SCSI register
uint32_t scsiGetRegTT(int whichReg)
{
    if(whichReg == REG_DMARES) {
        return *SDMARES;
    }

    if(whichReg == REG_DMACTL) {
        return *SDMACTL;
    }

    volatile uint8_t *pReg = (volatile uint8_t *) (0xFFFF8780 + whichReg);
    uint32_t val = *pReg;

    return val;
}

//----------------------
void scsi_setBit(int whichReg, uint32_t bitMask)
{
    uint32_t val;
    val = (*hdIf.pGetReg)(whichReg);         // read
    val = val | bitMask;                    // modify (set bits)
    (*hdIf.pSetReg)(whichReg, val);              // write
}

void scsi_clrBit(int whichReg, uint32_t bitMask)
{
    uint32_t val;
    uint32_t invMask = ~bitMask;

    val = (*hdIf.pGetReg)(whichReg);         // read
    val = val & invMask;                    // modify (clear bits)
    (*hdIf.pSetReg)(whichReg, val);              // write
}
//----------------------
uint8_t dmaDataTx_prepare_TT(uint8_t readNotWrite, uint8_t *buffer, uint32_t dataByteCount)
{
    // set DMA pointer to buffer address
    setDmaAddr_TT((uint32_t) buffer);

    // set DMA count
    setDmaCnt_TT(dataByteCount);

    return 0;
}
//----------------------
uint8_t dmaDataTx_do_TT(uint8_t readNotWrite, uint8_t *buffer, uint32_t dataByteCount)
{
    // Set up the DMAC for data transfer
    (*hdIf.pSetReg)(REG_Mode, 2);                      // enable DMA mode

    if(readNotWrite) {                          // on read
        (*hdIf.pSetReg)(REG_StartDmaInitiatorReceive, 0);                 // start the DMA receive
        (*hdIf.pSetReg)(REG_DMACTL, DMAIN);          // set the DMAC direction to IN
        (*hdIf.pSetReg)(REG_DMACTL, DMAIN+DMAENA);   // turn on DMAC
    } else {                                    // on write
        (*hdIf.pSetReg)(REG_StartDmaSend, 0);                 // start the DMA send -- WrSCSI  #0,SDS
        (*hdIf.pSetReg)(REG_DMACTL, DMAOUT);         // set the DMAC direction to OUT
        (*hdIf.pSetReg)(REG_DMACTL, DMAOUT+DMAENA);  // turn on DMAC
    }

    uint8_t res;
    res = scsiWaitForInt();                                  // wait for int
    if(res) {
        logMsg(" dmaDataTansfer() failed - scsiWaitForInt() timeout\r\n");
        return -1;
    }

    if(!readNotWrite) {                 // for WRITE the code end here, nothing more to do, just return good result
        return 0;
    }

    //--------------------------
    // the rest is only for the case of DMA read

	clearCache030();

    uint8_t rest = *bSDMAPTR_lo;   // see if this was an odd transfer
    rest = rest & 0x03;         // get only 2 lowest bits

    if(rest == 0) {             // transfer size was multiple of 4? Great, finish.
        return 0;
    }

    //----------------
    // the following code is only for case if the DMA read size (count) was not multiple of 4
    uint32_t dmaPtr;
    uint8_t *pData;
    dmaPtr  = getDmaAddr_TT();
    dmaPtr  = dmaPtr & 0xfffffffc;  // where does data go to?
    pData   = (uint8_t *) dmaPtr;      // int to pointer

    uint32_t residue = (*hdIf.pGetReg)(REG_DMARES);    // get the remaining bytes

    int i;
    for(i=0; i<rest; i++) {
        uint8_t val;
        val     = residue >> 24;        // get highest byte
        residue = residue << 8;         // shift next byte to highest byte

        *pData = val;                   // store byte and move to next position
        pData++;
    }

    return 0;
}
//----------------------
