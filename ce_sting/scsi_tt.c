// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
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
void logMsgProgress(DWORD current, DWORD total);

void scsi_reset(void);
//-----------------
// local function definitions
static BYTE scsi_select_and_cmd(BYTE readNotWrite, BYTE scsiId, BYTE *cmd, BYTE cmdLength, BYTE *dataAddr, DWORD dataByteCount);
static BYTE selscsi(BYTE scsiId);
static WORD dataTransfer(BYTE readNotWrite, BYTE *bfr, DWORD byteCount, BYTE cmdLength);
static void w4stat(void);
static BYTE doack(void);
       DWORD setscstmout(void);
       BYTE  w4req(void);

BYTE PIO_read(void);
void PIO_write(BYTE data);

#define USE_DMA

#ifdef USE_DMA
BYTE   w4int(void);
void   setDmaAddr_TT(DWORD addr);
DWORD  getDmaAddr_TT(void);
void   setDmaCnt_TT(DWORD dataCount);
#else
WORD pioDataTransfer(BYTE readNotWrite, BYTE *bfr, DWORD byteCount);
WORD pioDataTransfer_read(BYTE *bfr, DWORD byteCount);
WORD pioDataTransfer_write(BYTE *bfr, DWORD byteCount);
#endif

extern BYTE machine;

DWORD scsi_getReg_TT(int whichReg);
void  scsi_setReg_TT(int whichReg, DWORD value);

BYTE dmaDataTx_prepare_TT (BYTE readNotWrite, BYTE *buffer, DWORD dataByteCount);
BYTE dmaDataTx_do_TT      (BYTE readNotWrite, BYTE *buffer, DWORD dataByteCount);
//-----------------

void clearCache030(void);

DWORD _cmdTimeOut;                      // timeout time for scsi_cmd() from start to end
void scsi_cmd_TT(BYTE readNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount)
{
    //--------
    // init result to fail codes
    hdIf.success        = FALSE;
    hdIf.statusByte     = ACSIERROR;
    hdIf.phaseChanged   = FALSE;

    //-------------
    // create local copy of cmd[]
    BYTE tmpCmd[32];
    BYTE tmpCmdLen = (cmdLength < 32) ? cmdLength : 32;
    memcpy(tmpCmd, cmd, tmpCmdLen);

    //------------
    // first we start by extracting ID and fixing the cmd[] array because there's different format of this for ACSI and SCSI
    BYTE scsiId = (tmpCmd[0] >> 5);     // get only drive ID bits

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

    _cmdTimeOut = setscstmout();        // set up a short timeout
    BYTE res = scsi_select_and_cmd(readNotWrite, scsiId, cmd, cmdLength, buffer, sectorCount << 9);      // send command block

    if(res) {
        logMsg("scsi_cmd_tt failed on scsi_select_and_cmd() \r\n");
        *FLOCK = 0;                     // clear FLOCK to enable FDC operations

        hdIf.success = FALSE;
        return;
    }

    if(sectorCount != 0) {
        DWORD byteCount = sectorCount << 9;
        WORD wres = dataTransfer(readNotWrite, buffer, byteCount, cmdLength);

        if(wres) {
            logMsg("scsi_cmd_tt failed on dataTransfer \r\n");
            *FLOCK = 0;                 // clear FLOCK to enable FDC operations

            hdIf.success = FALSE;
            return;
        }
    }

    w4stat();                           // wait for status byte

    if(!hdIf.success) {
        logMsg("scsi_cmd_tt failed on w4stat \r\n");
    }

    *FLOCK = 0;                         // clear FLOCK to enable FDC operations
}

WORD dataTransfer(BYTE readNotWrite, BYTE *bfr, DWORD byteCount, BYTE cmdLength)
{
    WORD res;

    if(readNotWrite) {                                  // read
        (*hdIf.pSetReg)(REG_ICR, 0);                         // deassert the data bus
        (*hdIf.pSetReg)(REG_TCR, TCR_PHASE_DATA_IN);         // set DATA IN  phase
    } else {                                            // write
        (*hdIf.pSetReg)(REG_ICR, ICR_DBUS);                  // assert data bus
        (*hdIf.pSetReg)(REG_TCR, TCR_PHASE_DATA_OUT);        // set DATA OUT phase
    }

    res = (*hdIf.pGetReg)(REG_REI);             // clear potential interrupt

#ifdef USE_DMA                    // if using DMA for data transfer
    res = (*hdIf.pDmaDataTx_do) (readNotWrite, bfr, byteCount);
#else                           // if using PIO for data transfer
    res = pioDataTransfer(readNotWrite, bfr, byteCount);
#endif

    return res;
}

#ifndef USE_DMA

WORD pioDataTransfer(BYTE readNotWrite, BYTE *bfr, DWORD byteCount)
{
    WORD res;

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

WORD pioDataTransfer_read(BYTE *bfr, DWORD byteCount)
{
    int i;
    BYTE data;

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

WORD pioDataTransfer_write(BYTE *bfr, DWORD byteCount)
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

// scsi_select_and_cmd() - set DMA pointer and count and send command block
BYTE scsi_select_and_cmd(BYTE readNotWrite, BYTE scsiId, BYTE *cmd, BYTE cmdLength, BYTE *dataAddr, DWORD dataByteCount)
{
    BYTE res;

    res = selscsi(scsiId);  // select required device

    if(res) {               // if failed, quit with failure
        logMsg("scsi_select_and_cmd() failed on SELECT\r\n");
        return -1;
    }

#ifdef USE_DMA
    res = (*hdIf.pDmaDataTx_prepare)(readNotWrite, dataAddr, dataByteCount);

    if(res) {
        logMsg("scsi_select_and_cmd() failed on DmaDataTx_prepare()\r\n");
        return -1;
    }
#endif

    (*hdIf.pSetReg)(REG_TCR, TCR_PHASE_CMD);                // set COMMAND PHASE (assert C/D)
    (*hdIf.pSetReg)(REG_ICR, ICR_DBUS);                     // assert data bus

    int i;

    for(i=0; i<cmdLength; i++) {                            // send all the cmd bytes using PIO
        PIO_write(cmd[i]);

        if(!hdIf.success) {                                 // if time out happened, fail
            logMsg("scsi_select_and_cmd() - CMD phase failed on PIO_write()\r\n");
            logMsgProgress(i, cmdLength);
            return -1;
        }
    }

    return 0;
}

// Selects the SCSI device with specified SCSI ID
BYTE selscsi(BYTE scsiId)
{
    BYTE res;

    while(1) {                                              // STILL busy from last time?
        BYTE icr = (*hdIf.pGetReg)(REG_CR);
        if((icr & ICR_BUSY) == 0) {                         // if not, it's available
            break;
        }

        DWORD now = *HZ_200;
        if(now >= _cmdTimeOut) {                            // if time out, fail
            return -1;
        }
    }

    (*hdIf.pSetReg)(REG_TCR, TCR_PHASE_DATA_OUT);           // data out phase
    (*hdIf.pSetReg)(REG_ISR, 0);                            // no interrupt from selection
    (*hdIf.pSetReg)(REG_ICR, ICR_BSY | ICR_SEL);            // assert BSY and SEL

    BYTE selId  = (1 << scsiId);                            // convert number of device to bit
    (*hdIf.pSetReg)(REG_ODR, selId);                        // set dest SCSI IDs

    (*hdIf.pSetReg)(REG_ICR, ICR_BSY | ICR_SEL | ICR_DBUS); // assert BUSY, SEL and data bus
    scsi_clrBit(REG_MR, MR_ARBIT);                          // clear arbitrate bit
    scsi_clrBit(REG_ICR, ICR_BSY);                          // clear BUSY

    while(1) {                          // wait for busy bit to appear
        BYTE icr = (*hdIf.pGetReg)(REG_CR);

        if(icr & ICR_BUSY) {            // if bit set, good
            res = 0;
            break;
        }

        DWORD now = *HZ_200;
        if(now >= _cmdTimeOut) {                            // if time out, fail
            res = -1;
            break;
        }
    }

    (*hdIf.pSetReg)(REG_ICR, 0);                            // clear SEL and data bus assertion
    return res;
}

void scsi_reset(void)
{
    (*hdIf.pSetReg)(REG_ICR, ICR_RST);                      // assert RST

    _cmdTimeOut = *HZ_200 + 100;                            // wait 0.5 s

    DWORD now;
    while(1) {
        now = *HZ_200;

        if(now >= _cmdTimeOut) {
            break;
        }
    }

    (*hdIf.pSetReg)(REG_ICR, 0);                            // back to normal

    _cmdTimeOut = *HZ_200 + 100;                            // wait 0.5 s

    while(1) {
        now = *HZ_200;

        if(now >= _cmdTimeOut) {
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
        if(res & GPIP2_NCR) {           // NCR 5380 interrupt?
            break;
        }

        if((res & GPIP2_DMA) == 0) {    // DMA interrupt?
            WORD wres = (*hdIf.pGetReg)(REG_DMACTL);    // get the DMAC status
            if(wres & 0x80) {           // check for bus err/ignore cntout ints
                return -1;
            }
        }

        DWORD now = *HZ_200;
        if(now >= _cmdTimeOut) {            // time out? fail
            return -1;
        }
    }

    (*hdIf.pGetReg)(REG_REI);               // clear potential interrupt
    (*hdIf.pSetReg)(REG_DMACTL, DMADIS);    // disable DMA
    (*hdIf.pSetReg)(REG_MR,  0);            // disable DMA mode
    (*hdIf.pSetReg)(REG_ICR, 0);            // make sure data bus is not asserted

    return 0;
}

// w4stat - wait for status byte and message byte.
void w4stat(void)
{
	(*hdIf.pSetReg)(REG_TCR, TCR_PHASE_STATUS);     // STATUS IN phase
	(*hdIf.pGetReg)(REG_REI);                       // clear potential interrupt

    //-----------------
    // receive status byte
    BYTE status = PIO_read();

    if(!hdIf.success) {                             // failed?
        logMsg("w4stat failed on reading status byte \r\n");
        return;
    }

    //-----------------
    // receive message byte
	(*hdIf.pSetReg)(REG_TCR, TCR_PHASE_MESSAGE_IN); // MESSAGE IN phase
	(*hdIf.pGetReg)(REG_REI);                       // clear potential interrupt

    (void) PIO_read();
    if(!hdIf.success) {
        logMsg("w4stat failed on reading message in \r\n");
        return;
    }

    hdIf.success        = TRUE;                     // success!
    hdIf.statusByte     = status;                   // store status byte
}

BYTE PIO_read(void)
{
    BYTE res;
    (*hdIf.pSetReg)(REG_ICR, 0);         // deassert data bus (disable data output)

    hdIf.phaseChanged   = FALSE;
    hdIf.success        = FALSE;

    res = w4req();                      // wait for status byte
    if(res) {                           // if timed-out, fail
        logMsg("PIO_read() - fail on w4req() \r\n");
        return 0;
    }

    res = (*hdIf.pGetReg)(REG_DSR);
    if((res & (1 << 3)) == 0) {         // PHASE MATCH bit from BUS AND STATUS REGISTER is low? SCSI phase changed
//        logMsg("PIO_read() - phase change \r\n");

        hdIf.phaseChanged   = TRUE;
        return 0;
    }

    BYTE data = (*hdIf.pGetReg)(REG_DB); // get the status byte

    res = doack();                      // signal that status byte is here
    if(res) {                           // if timed-out, fail
        logMsg("PIO_read() - fail on doack() \r\n");
        return 0;
    }

    hdIf.success = TRUE;
    return data;
}

void PIO_write(BYTE data)
{
    BYTE res;

    hdIf.phaseChanged   = FALSE;
    hdIf.success        = FALSE;

    res = w4req();                      // wait for status byte
    if(res) {                           // if timed-out, fail
        logMsg("PIO_write() - fail on w4req() \r\n");
        return;
    }

    res = (*hdIf.pGetReg)(REG_DSR);
    if((res & (1 << 3)) == 0) {         // PHASE MATCH bit from BUS AND STATUS REGISTER is low? SCSI phase changed
//        logMsg("PIO_write() - phase change \r\n");

        hdIf.phaseChanged = FALSE;
        return;
    }

    (*hdIf.pSetReg)(REG_ICR, ICR_DBUS);  // assert data bus (enable data output)
    (*hdIf.pSetReg)(REG_DB, data);

    res = doack();                      // signal that status byte is here
    if(res) {                           // if timed-out, fail
        logMsg("PIO_write() - fail on doack() \r\n");
        return;
    }

    hdIf.success = TRUE;
}

// w4req() - wait for REQ to come during hand shake of non-data bytes
BYTE w4req(void)
{
    while(1) {                      // wait for REQ
        BYTE icr = (*hdIf.pGetReg)(REG_CR);
        if(icr & ICR_REQ) {         // if REQ appeared, good
            return 0;
        }

        DWORD now = *HZ_200;
        if(now >= _cmdTimeOut) {    // if time out, fail
            break;
        }
    }

    return -1;                      // time out
}

// doack() - assert ACK
BYTE doack(void)
{
    scsi_setBit(REG_ICR, ICR_ACK);   // assert ACK

    BYTE res;

    while(1) {
        BYTE icr = (*hdIf.pGetReg)(REG_ICR);
        if((icr & ICR_REQ) == 0) {      // if REQ gone, good
            res = 0;
            break;
        }

        DWORD now = *HZ_200;
        if(now >= _cmdTimeOut) {        // if time out, fail
            res = -1;
            break;
        }
    }

    scsi_clrBit(REG_ICR, ICR_ACK);      // clear ACK
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
void scsi_setBit(int whichReg, DWORD bitMask)
{
    DWORD val;
    val = (*hdIf.pGetReg)(whichReg);         // read
    val = val | bitMask;                    // modify (set bits)
    (*hdIf.pSetReg)(whichReg, val);              // write
}

void scsi_clrBit(int whichReg, DWORD bitMask)
{
    DWORD val;
    DWORD invMask = ~bitMask;

    val = (*hdIf.pGetReg)(whichReg);         // read
    val = val & invMask;                    // modify (clear bits)
    (*hdIf.pSetReg)(whichReg, val);              // write
}
//----------------------
BYTE dmaDataTx_prepare_TT(BYTE readNotWrite, BYTE *buffer, DWORD dataByteCount)
{
    // set DMA pointer to buffer address
    setDmaAddr_TT((DWORD) buffer);

    // set DMA count
    setDmaCnt_TT(dataByteCount);

    return 0;
}
//----------------------
BYTE dmaDataTx_do_TT(BYTE readNotWrite, BYTE *buffer, DWORD dataByteCount)
{
    // Set up the DMAC for data transfer
    (*hdIf.pSetReg)(REG_MR, 2);                      // enable DMA mode

    if(readNotWrite) {                          // on read
        (*hdIf.pSetReg)(REG_DIR, 0);                 // start the DMA receive
        (*hdIf.pSetReg)(REG_DMACTL, DMAIN);          // set the DMAC direction to IN
        (*hdIf.pSetReg)(REG_DMACTL, DMAIN+DMAENA);   // turn on DMAC
    } else {                                    // on write
        (*hdIf.pSetReg)(REG_SDS, 0);                 // start the DMA send -- WrSCSI  #0,SDS
        (*hdIf.pSetReg)(REG_DMACTL, DMAOUT);         // set the DMAC direction to OUT
        (*hdIf.pSetReg)(REG_DMACTL, DMAOUT+DMAENA);  // turn on DMAC
    }

    BYTE res;
    res = w4int();                                  // wait for int
    if(res) {
        logMsg(" dmaDataTansfer() failed - w4int() timeout\r\n");
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

    DWORD residue = (*hdIf.pGetReg)(REG_DMARES);    // get the remaining bytes

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
//----------------------
