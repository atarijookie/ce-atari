#include <Ethernet.h>

#include "defs.h"
#include "bridge.h"
#include "utils.h"
#include "command_handling.h"
#include "connection.h"
#include "scsi.h"
#include "ipc.h"

extern EthernetClient clientHdd;
extern EthernetClient clientIkbd;

uint8_t onGetCommandScsi(void);
void getCmdLengthFromCmdBytesScsi(uint8_t cmd);

extern uint8_t atnSendACSIcommand[ATN_SENDACSICOMMAND_LEN_TX];

//----------
uint8_t cmd[16];   // received command bytes, should point beyond the header in atnSendACSIcommand
uint8_t cmdLen; // length of received command
uint8_t brStat; // status from bridge

extern uint8_t busIdle;

uint8_t onGetCommand(void)
{
    // retrieve the comman
    uint8_t good = onGetCommandScsi();

    if (!good)  // if failed to get the cmd, quit
    {
        timeoutClear();
        return STATE_GET_COMMAND;
    }

    IPCbuffer* bfr = ipcGetFreeBuffer(0, CMD_TIMEOUT_SHORT);
    if(bfr == NULL) {
        debug("onGetCommand - no free buffers\n");
        timeoutClear();
        return STATE_GET_COMMAND;
    }

    // store data to buffer, add buffer index to queue
    ipcSetBufferAndPutToFifo(bfr, 0, STATE_GET_COMMAND, cmdLen, cmd, cmdLen);

    //----------------
    // command received, send it to host
    timeoutStart(); // start the timeout timer to give the rest of code full timeout time

    return STATE_WAIT_COMMAND_RESPONSE;
}

void doMsgOutIfAtnSet(void)
{
    while(1)
    {
        int atnReset = getAtnReset();

        if((atnReset & BIT_ATN) != BIT_ATN) {   // ATN not set? quit
            return;
        }

        // temporarily set MSG_OUT mode
        int originalMode = pioConfig(MODE_MSG_OUT);
        PIO_write();            // do 1 PIO write, ignore retrieved MSG byte

        // restore original mode
        pioConfig(originalMode);
    }
}

uint8_t onGetCommandScsi(void)
{
    uint8_t id = getSelectionByte(); // check if one of the IDs of this device is selected

    if (id == 0xff)     // this device was not selected?
    {
        return 0;
    }

    // TODO: possibly re-enable
    // doMsgOutIfAtnSet();     // do MSG_OUT if ATN set before cmd transfer

    pioConfig(MODE_CMD);

    // dump_gpio(PIN_RST_CD_REQ_SCL);  // TODO: remove
    // dump_gpio(PIN_OUT_OE);  // TODO: remove
    // dump_gpio(PIN_OUT_LE2);  // TODO: remove

    cmdLen = 6; // maximum 6 bytes at start, but this might change in getCmdLengthFromCmdBytes()

    for (int i = 0; i < cmdLen; i++)
    {                         // receive the next command bytes
        cmd[i] = PIO_write(); // drop down IRQ, get byte

        if (brStat != E_OK)
        { // if something was wrong, quit, failed
            debug("FAIL cmd i: %d\n", i);
            resetBridge();
            return 0;
        }

        if (i == 0)
        {                                         // if we got also the 2nd byte
            getCmdLengthFromCmdBytesScsi(cmd[0]); // we set up the length of command, etc.
        }
    }

    // now fix the command if the length is more than 6 bytes
    if (cmdLen > 6)
    {
        for (int i = 13; i > 0; i--)
        { // move the cmd one byte further (to make cmd[0] unused)
            cmd[i] = cmd[i - 1];
        }
        cmd[0] = 0x1f; // store ICD command marker

        cmdLen++; // now the command is one byte longer
    }

    // for all commands add fake ACSI ID on top of the 0th byte
    cmd[0] = cmd[0] | (id << 5); // add ID on the top 3 bits

    // doMsgOutIfAtnSet();     // do MSG_OUT if ATN set after cmd transfer

    return 1;
}

extern int readInProgressCount;

bool onDataRead(uint32_t cnt, uint8_t* bfr)
{
#ifdef LOG_MORE
    // debug("onDataRead dataCnt: %d\n", cnt);
    // debug("onDataRead START readInProgressCount: %d\n", readInProgressCount);
#endif

    pioConfig(MODE_DMA_READ);

    for(uint32_t i=0; i<cnt; i++) {    // send all the data from buffer to Atari
        DMA_read(bfr[i]);

        if (brStat == E_TimeOut)
        {
            debug("onDataRead TO 1\n");
            return false;
        }
    }

    DMA_read_waitForEnd();

    if (brStat == E_TimeOut)        // read failed to wait for end?
    {
        debug("onDataRead TO 2\n");
        return false;
    }

    return true;
}

bool onDataWrite(uint32_t dataCnt)
{
    // debug("onDataWrite dataCnt: %d\n", dataCnt);

    // create and send one header at the start
    IPCbuffer* bfr = ipcGetFreeBuffer(0, CMD_TIMEOUT_SHORT);
    if(bfr) {
        ipcSetBufferAndPutToFifo(bfr, 0, STATE_SEND_WRITE_MORE_DATA, dataCnt, NULL, 0);
    } else {
        debug("onDataWrite - ipcGetFreeBuffer failed\n");
        return false;
    }

    pioConfig(MODE_DMA_WRITE);

    DMA_write_startWithCount(dataCnt);      // let PIO program know the count of bytes we want to transfer

    while (dataCnt > 0)             // something to write?
    {
        uint32_t cntNow = MIN(dataCnt, 512);

        dataCnt -= cntNow;

        // get buffer where we can store the written data
        IPCbuffer* bfr = ipcGetFreeBuffer(0, CMD_TIMEOUT_SHORT);
        if(!bfr) {
            debug("onDataWrite - ipcGetFreeBuffer failed\n");
            return false;
        }
        bfr->free = false;

        for(int i = 0; i < cntNow; i++)
        {
            bfr->data[i] = DMA_write();          // get data from Atari

            if (brStat == E_TimeOut)
            {                              // if timeout occured
#ifdef LOG_MORE
    debug("onDataWrite timeout on DMA_write");
#endif
                return false; // transfer failed, don't send status, just get next command
            }
        }

        ipcSetBufferAndPutToFifo(bfr, 0, STATE_DATA_WRITE, cntNow, NULL, 0);
    }

    return true;  // continue with sending the status
}

void onReadStatus(uint8_t statusByte)
{
    statusAndMsgRead(statusByte);       // send the status to Atari
}

void getCmdLengthFromCmdBytesScsi(uint8_t cmd)
{
    switch ((cmd & 0xe0) >> 5) // get the length of the command
    {
    case 0:
        cmdLen = 6;
        break;
    case 1:
        cmdLen = 10;
        break;
    case 2:
        cmdLen = 10;
        break;
    case 4:
        cmdLen = 16;
        break;
    case 5:
        cmdLen = 12;
        break;
    default:
        cmdLen = 6;
        break;
    }
}

