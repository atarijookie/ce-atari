#include "defs.h"
#include "bridge.h"
#include "utils.h"
#include "command_handling.h"

void onButtonPress(void);

void processHostCommands(void);
void handleAcsiConfig(uint8_t newAcsiIds);

uint8_t sendBufferToHost(uint8_t *bfr, uint32_t txCount);

uint8_t onGetCommandAcsi(void);
uint8_t onGetCommandScsi(void);
void getCmdLengthFromCmdBytesAcsi(void);
void getCmdLengthFromCmdBytesScsi(uint8_t cmd);

void onGetCommand(void);
void onDataRead(uint8_t withStatus);
void onDataWrite(void);
void onReadStatus(void);

extern uint8_t state;
extern uint32_t dataCnt;
extern uint8_t statusByte;

extern uint8_t atnSendACSIcommand[ATN_SENDACSICOMMAND_LEN_TX];

extern uint16_t seqNo;
extern uint8_t atnMoreData[ATN_READMOREDATA_LEN_TX];

extern uint8_t atnGetStatus[ATN_GETSTATUS_LEN_TX];

extern TWriteBuffer wrBuf1, wrBuf2;
extern TReadBuffer rdBuf1, rdBuf2;
extern uint16_t smallDataBuffer[2];

extern uint8_t cmdBuffer[CMD_BUFFER_LENGTH];

//----------
extern uint8_t *cmd;   // received command bytes, should point beyond the header in atnSendACSIcommand
extern uint8_t cmdLen;  // length of received command
extern uint8_t brStat;  // status from bridge
extern uint8_t lastScsiStatusByte;

extern uint8_t enabledIDs;

extern uint8_t firstConfigReceived; // used to turn LEDs on after first config received
extern uint8_t shouldProcessCommands;

extern uint8_t isAcsiNotScsi;
extern uint8_t busIdle;

void startSpiDmaForDataRead(uint32_t dataCnt, TReadBuffer *readBfr);

void onGetCommand(void)
{
    uint8_t i, id;

    //---------
    // retrieve the command. There are some slight differences between ACSI and SCSI part,
    // but the resulting commands should be the same (to make the rest of app work without further changes).
    uint8_t good;

    if (isAcsiNotScsi)
    { // for ACSI
        good = onGetCommandAcsi();
    }
    else
    { // for SCSI
        good = onGetCommandScsi();
    }

    if (!good)
    { // if failed to get the cmd, quit
        return;
    }

    id = (cmd[0] >> 5) & 0x07; // get only device ID

    //-----
    if(!idIsEnabled(id))        // this ID not enabled, ignore command
    {
        return;
    }

    //----------------
    // if we got here, we should handle this in host
    timeoutStart(); // start the timeout timer to give the rest of code full timeout time

    sendBufferToHost(atnSendACSIcommand, ATN_SENDACSICOMMAND_LEN_TX);

    state = STATE_WAIT_COMMAND_RESPONSE;
    shouldProcessCommands = TRUE; // mark that we should process the commands on next SPI DMA idle time
}

uint8_t onGetCommandAcsi(void)
{
    uint8_t id, i;

    //----------------------
    cmd[0] = PIO_writeFirst(); // get byte from ST (waiting for the 1st byte)
    id = (cmd[0] >> 5) & 0x07; // get only device ID

    //----------------------
    if(!idIsEnabled(id)) // if this ID is not enabled, quit
    {
        return 0;
    }

    cmdLen = 6; // maximum 6 bytes at start, but this might change in getCmdLengthFromCmdBytes()

    for (i = 1; i < cmdLen; i++)
    {                         // receive the next command bytes
        cmd[i] = PIO_write(); // drop down IRQ, get byte

        if (brStat != E_OK)
        { // if something was wrong, quit, failed
            resetBridge();
            return 0;
        }

        if (i == 1)
        {                                   // if we got also the 2nd byte
            getCmdLengthFromCmdBytesAcsi(); // we set up the length of command, etc.
        }
    }

    return 1;
}

uint8_t onGetCommandScsi(void)
{
    uint8_t id;
    uint8_t sel;
    int i;

    //----------------------
    sel = PIO_writeFirst(); // get SELection byte
    id = 0xff;              // mark that ID hasn't been found yet

    for (i = 0; i < 8; i++)
    {
        if ((sel & (1 << i)) != 0)
        { // if bit is one, this ID is selected
            if(idIsEnabled(id))
            {           // if that ID is enabled
                id = i; // store this ID and quit loop
                break;
            }
        }
    }

    if (id == 0xff)
    { // ID not found? quit
        return 0;
    }
    //----------------------
    if(!idIsEnabled(id))    // if this ID is not enabled, quit
    {
        return 0;
    }

    cmdLen = 6; // maximum 6 bytes at start, but this might change in getCmdLengthFromCmdBytes()

    for (i = 0; i < cmdLen; i++)
    {                         // receive the next command bytes
        cmd[i] = PIO_write(); // drop down IRQ, get byte

        if (brStat != E_OK)
        { // if something was wrong, quit, failed
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
        for (i = 13; i > 0; i--)
        { // move the cmd one byte further (to make cmd[0] unused)
            cmd[i] = cmd[i - 1];
        }
        cmd[0] = 0x1f; // store ICD command marker

        cmdLen++; // now the command is one byte longer
    }

    // for all commands add fake ACSI ID on top of the 0th byte
    cmd[0] = cmd[0] | (id << 5); // add ID on the top 3 bits
    return 1;
}

void onDataRead(uint8_t withStatus)
{
    // uint16_t i, loopCount, l, dataBytesCount;
    // uint8_t dataMarkerFound;
    // uint8_t res;
    // TReadBuffer *rdBufNow;
    // uint16_t *pData;

    // seqNo = 0;
    // state = STATE_GET_COMMAND; // this will be the next state once this function finishes with fail

    // // nothing to send AND should send status? then just quit with status byte
    // if (dataCnt == 0 && withStatus)
    // {
    //     PIO_read(statusByte);
    //     return;
    // }

    // // calculate how many loops we will have to do
    // loopCount = dataCnt / 512;

    // if ((dataCnt % 512) != 0)
    // {
    //     loopCount++;
    // }

    // // receive 0th data block in rdBuf1
    // rdBufNow = &rdBuf1;

    // startSpiDmaForDataRead(dataCnt, rdBufNow);
    // dataCnt -= (uint32_t)rdBufNow->dataBytesCount; // update remaining data size

    // // now start the double buffered transfer to ST
    // setDataDirection(DIR_SEND); // data direction for reading

    // for (l = 0; l < loopCount; l++)
    // {
    //     // first wait until all data arrives in SPI DMA transfer
    //     while (!spiDmaIsIdle)
    //     {
    //         if (timeout())
    //         {                               // if the data from host doesn't come within timeout, quit
    //             setDataDirection(DIR_RECV); // data direction for writing, and quit
    //             return;
    //         }
    //     }

    //     // if after transfering this block there should be another block of data
    //     if (dataCnt > 0)
    //     {
    //         TReadBuffer *nextRdBuffer = rdBufNow->next;

    //         startSpiDmaForDataRead(dataCnt, nextRdBuffer);     // start receiving data to the other buffer
    //         dataCnt -= (uint32_t)nextRdBuffer->dataBytesCount; // update remaining data size
    //     }

    //     ///////////////////////////////////////////////////////////////
    //     // send the received data to ST
    //     // find the data marker
    //     dataMarkerFound = FALSE;
    //     pData = &rdBufNow->buffer[0];

    //     for (i = 0; i < rdBufNow->count; i++)
    //     {
    //         uint16_t data;

    //         data = *pData; // get data
    //         pData++;

    //         if (data == CMD_DATA_MARKER)
    //         { // found data marker?
    //             dataMarkerFound = TRUE;
    //             break;
    //         }
    //     }

    //     if (dataMarkerFound == FALSE)
    //     { // didn't find the data marker?
    //         if (withStatus)
    //         {
    //             PIO_read(SCSI_ST_CHECK_CONDITION); // send status: CHECK CONDITION and quit
    //         }
    //         return;
    //     }

    //     // now try to trasmit the data
    //     dataBytesCount = rdBufNow->dataBytesCount;

    //     res = dataReadCloop(pData, dataBytesCount);

    //     if (res == 0)
    //     {
    //         setDataDirection(DIR_RECV); // data direction for writing, and quit
    //         return;
    //     }

    //     // one cycle finished, now swap buffers and start all over again
    //     rdBufNow = rdBufNow->next; // swap buffers
    // }

    // if (withStatus)
    // { // if should send status, then send status and go to STATE_GET_COMMAND
    //     state = STATE_GET_COMMAND;
    //     PIO_read(statusByte); // send the status to Atari
    // }
    // else
    // { // if shouldn't send status here, switch to state STATE_READ_STATUS, which will retrieve status from host and send it to ST
    //     state = STATE_READ_STATUS;
    // }
}

void onDataWrite(void)
{
    // uint8_t firstLoop, previousSpiSuccess;
    // uint16_t subCount, recvCount;
    // uint16_t index, data, i, value;
    // TWriteBuffer *wrBufNow;

    // seqNo = 0;
    // wrBufNow = &wrBuf1;

    // setDataDirection(DIR_RECV); // data direction for reading

    // firstLoop = TRUE;

    // while (dataCnt > 0)
    // { // something to write?
    //     // request maximum 512 bytes from host
    //     subCount = (dataCnt > 512) ? 512 : dataCnt;
    //     dataCnt -= subCount;

    //     wrBufNow->buffer[4] = seqNo; // set the sequence # to Attention
    //     seqNo++;

    //     recvCount = subCount / 2;    // uint16_ts to receive: convert # of uint8_ts to # of uint16_ts
    //     recvCount += (subCount & 1); // if subCount is odd number, then we need to transfer 1 uint16_t more

    //     index = 5; // length of header before data

    //     for (i = 0; i < recvCount; i++)
    //     {                        // write this many uint16_ts
    //         value = DMA_write(); // get data from Atari
    //         value = value << 8;  // store as upper byte

    //         if (brStat == E_TimeOut)
    //         {                              // if timeout occured
    //             state = STATE_GET_COMMAND; // transfer failed, don't send status, just get next command
    //             return;
    //         }

    //         subCount--;
    //         if (subCount == 0)
    //         {                                    // in case of odd data count
    //             wrBufNow->buffer[index] = value; // store data
    //             index++;

    //             break;
    //         }

    //         data = DMA_write();   // get data from Atari
    //         value = value | data; // store as lower byte

    //         if (brStat == E_TimeOut)
    //         {                              // if timeout occured
    //             state = STATE_GET_COMMAND; // transfer failed, don't send status, just get next command
    //             return;
    //         }

    //         subCount--;

    //         wrBufNow->buffer[index] = value; // store data
    //         index++;
    //     }

    //     wrBufNow->buffer[index] = 0; // terminating zero
    //     wrBufNow->count = index + 1; // store count, +1 because we have terminating zero

    //     //----------
    //     // set up the SPI DMA transfer
    //     previousSpiSuccess = sendBufferToHost(wrBufNow->buffer, wrBufNow->count * 2);

    //     // if this is not the first loop and the previous SPI transfer failed (something from this WRITE command was not transfered to host), fail
    //     if (!firstLoop && !previousSpiSuccess)
    //     {
    //         state = STATE_GET_COMMAND;
    //         return;
    //     }

    //     firstLoop = FALSE;
    //     //----------

    //     wrBufNow = wrBufNow->next; // use next write buffer
    // }

    state = STATE_READ_STATUS; // continue with sending the status
}

void onReadStatus(void)
{
    uint8_t i, newStatus;

    newStatus = 0xff; // no status received

    sendBufferToHost(&atnGetStatus[0], ATN_GETSTATUS_LEN_TX * 2);

    // spiDma_waitForFinish();

    // for (i = 0; i < 8; i++)
    // { // go through the received buffer
    //     if (cmdBuffer[i] == CMD_SEND_STATUS)
    //     {
    //         newStatus = cmdBuffer[i + 1] >> 8;
    //         break;
    //     }
    // }

    PIO_read(newStatus);       // send the status to Atari
    state = STATE_GET_COMMAND; // get the next command
}

void getCmdLengthFromCmdBytesAcsi(void)
{
    // now it's time to set up the receiver buffer and length
    if ((cmd[0] & 0x1f) == 0x1f)
    {                                 // if the command is '0x1f'
        switch ((cmd[1] & 0xe0) >> 5) // get the length of the command
        {
        case 0:
            cmdLen = 7;
            break;
        case 1:
            cmdLen = 11;
            break;
        case 2:
            cmdLen = 11;
            break;
        case 5:
            cmdLen = 13;
            break;
        default:
            cmdLen = 7;
            break;
        }
    }
    else
    {               // if it isn't a ICD command
        cmdLen = 6; // then length is 6 bytes
    }
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

uint8_t idIsEnabled(uint8_t id)
{
    if(id > 7) {
        return FALSE;
    }

    return (enabledIDs & (1 << id));
}
