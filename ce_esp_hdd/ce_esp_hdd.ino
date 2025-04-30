#include "defs.h"
#include "bridge.h"
#include "utils.h"

void onButtonPress(void);

void processHostCommands(void);
void handleAcsiConfig(uint8_t acsiIds);

uint8_t sendBufferToHost(uint8_t *bfr, uint32_t txCount);

uint8_t onGetCommandAcsi(void);
uint8_t onGetCommandScsi(void);
void getCmdLengthFromCmdBytesAcsi(void);
void getCmdLengthFromCmdBytesScsi(uint8_t cmd);

void onGetCommand(void);
void onDataRead(uint8_t withStatus);
void onDataWrite(void);
void onReadStatus(void);

uint8_t state;
uint32_t dataCnt;
uint8_t statusByte;

uint16_t version[2] = {0xa025, 0x0430}; // this means: hAns, 2025-04-30

char *VERSION_STRING_SHORT = {"4.00"};
char *DATE_STRING = {"04/30/25"}; // MM/DD/YY

volatile uint8_t sendFwVersion;
uint8_t atnSendFwVersion[ATN_SENDFWVERSION_LEN_TX * 2];
uint8_t atnSendACSIcommand[ATN_SENDACSICOMMAND_LEN_TX * 2];

uint16_t seqNo = 0;
uint8_t atnMoreData[ATN_READMOREDATA_LEN_TX * 2];

uint8_t atnGetStatus[ATN_GETSTATUS_LEN_TX * 2];

TWriteBuffer wrBuf1, wrBuf2;
TReadBuffer rdBuf1, rdBuf2;
uint16_t smallDataBuffer[2];

uint8_t cmdBuffer[CMD_BUFFER_LENGTH];

//----------
uint8_t cmd[14]; // received command bytes
uint8_t cmdLen;  // length of received command
uint8_t brStat;  // status from bridge
uint8_t lastScsiStatusByte;

uint8_t enabledIDs[8]; // when 1, Hanz will react on that ACSI ID #

uint8_t firstConfigReceived; // used to turn LEDs on after first config received
uint8_t shouldProcessCommands;

uint32_t isrNow, isrPrev;

uint16_t prevBtnPressTime;

void handleAcsiCommand(void);

uint8_t isAcsiNotScsi;
uint8_t busIdle;

uint32_t toStart;
uint32_t lastStart, lastEnd;

uint32_t lastSendFwTime;

uint8_t btnDownTime;

struct
{
    uint16_t acsi;
} configWords;

void sendFwToHost(void);
//--------------------------
void setup(void)
{
#define INPUTS_COUNT 12
    int inputs[INPUTS_COUNT] = {PIN_D0, PIN_D1, PIN_D2, PIN_D3, PIN_D4, PIN_D5, PIN_D6, PIN_D7, PIN_CMD1ST, PIN_EOT, PIN_SDA, PIN_BOOT_BTN};

    for (int i = 0; i < INPUTS_COUNT; i++)
    {
        pinMode(inputs[i], INPUT);
    }

#define OUTPUTS_COUNT 6
    int outputs[OUTPUTS_COUNT] = {PIN_OUT_OE, PIN_FF12D, PIN_INT_TRIG, PIN_DRQ_TRIG, PIN_SCL, PIN_CMD_DATA};

    for (int i = 0; i < OUTPUTS_COUNT; i++)
    {
        pinMode(outputs[i], OUTPUT);
    }

    state = STATE_GET_COMMAND;

    sendFwVersion = FALSE;
    firstConfigReceived = FALSE; // used to turn LEDs on after first config received

    setupAtnBuffers(); // fill the ATN buffers with needed headers and terminators

    shouldProcessCommands = FALSE;
    prevBtnPressTime = 0;

    getBridgeStatus();
    resetBridge();

    lastSendFwTime = millis();
}

void loop(void)
{
    //-------------
    // main loop
    while (1)
    {
        //---------------------------
        // get the command from ACSI and send it to host
        if (PIO_gotFirstCmdByte())
        { // if 1st CMD byte was received
            handleAcsiCommand();
        }

        //---------------------------
        // sending and receiving data over SPI using DMA
        if (shouldProcessCommands)
        {                          // SPI DMA: nothing to Tx and nothing to Rx?
            processHostCommands(); // and process all the received commands

            shouldProcessCommands = FALSE; // mark that we don't need to process commands until next time
        }

        // in command waiting state, nothing to do and should send FW version?
        uint32_t now = millis();

        if ((now - lastSendFwTime) >= 1000)
        {
            sendFwToHost();
        }

        //---------------------------
        // if the button was pressed, handle it
        // if(EXTI->PR & BUTTON) {
        //     onButtonPress();
        // }
    }
}

void sendFwToHost(void)
{
    sendFwVersion = FALSE;
    sendBufferToHost((uint8_t *)&atnSendFwVersion[0], ATN_SENDFWVERSION_LEN_TX * 2);
    shouldProcessCommands = TRUE;
}

void handleAcsiCommand(void)
{
    // this is the loop which should go through the whole command processing (cmd phase, data phase, status phase) without other stuff
    while (1)
    {
        // get the command from ACSI and send it to host
        // IN  STATE: STATE_GET_COMMAND
        // OUT STATE: WAIT_COMMAND_RESPONSE when GOOD, STATE_GET_COMMAND when FAIL
        if (state == STATE_GET_COMMAND && PIO_gotFirstCmdByte())
        { // if 1st CMD byte was received
            onGetCommand();
        }

        // transfer the data - read (to ST)
        // IN  STATE: STATE_DATA_READ_WITH_STATUS
        // OUT STATE: always STATE_GET_COMMAND, but if everything is well, it also does PIO_read()
        // ...or...
        // IN  STATE: STATE_DATA_READ_WITHOUT_STATUS
        // OUT STATE: STATE_READ_STATUS on success, STATE_GET_COMMAND on FAIL (just like STATE_DATA_WRITE)
        if (state == STATE_DATA_READ_WITH_STATUS || state == STATE_DATA_READ_WITHOUT_STATUS)
        {
            longTimeout_basedOnSectorCount(dataCnt >> 9); // set timeout time based on how many sectors are transfered

            onDataRead(state == STATE_DATA_READ_WITH_STATUS);

            timerSetup_cmdTimeoutChangeLength(CMD_TIMEOUT_SHORT); // after data transfer restore short timeout value
            break;                                                // at this point it's either success or fail, but we're finished here
        }

        // transfer the data - write (from ST)
        // IN  STATE: STATE_DATA_WRITE
        // OUT STATE: STATE_READ_STATUS on success, STATE_GET_COMMAND on FAIL
        if (state == STATE_DATA_WRITE)
        {
            longTimeout_basedOnSectorCount(dataCnt >> 9); // set timeout time based on how many sectors are transfered

            onDataWrite();

            timerSetup_cmdTimeoutChangeLength(CMD_TIMEOUT_SHORT); // after data transfer restore short timeout value
        }

        // this happens after WRITE - wait for status byte, send it to ST (read)
        // IN  STATE: STATE_READ_STATUS
        // OUT STATE: always STATE_GET_COMMAND
        if (state == STATE_READ_STATUS)
        {
            timeoutStart(); // start the timeout timer to give the rest of code full timeout time

            onReadStatus();
            break; // at this point it's either success or fail, but we're finished here
        }

        // sending and receiving data over SPI using DMA
        // IN  STATE: any
        // OUT STATE: STATE_DATA_WRITE, STATE_DATA_READ_WITH_STATUS, STATE_DATA_READ_WITHOUT_STATUS, or unchanged
        if (spiDmaIsIdle && shouldProcessCommands)
        {                          // SPI DMA: nothing to Tx and nothing to Rx?
            processHostCommands(); // and process all the received commands

            shouldProcessCommands = FALSE; // mark that we don't need to process commands until next time
        }

        if (timeout())
        { // if the data from host doesn't come within timeout, quit
            LOG_ERROR(50);
            state = STATE_GET_COMMAND;
            break;
        }

        // if we came here and we are in the basic state, go to the outside loop to do the rest of the code
        if (state == STATE_GET_COMMAND)
        {
            break;
        }
    }

    //---------------
    // if something was wrong, reset XILINX so it won't get stuck
    if (brStat != E_OK)
    {
        resetBridge();
    }

    // The following goes only for SCSI interface, because current getBridgeStatus() (which is called from isBusIdle())
    // triggers INT going low, and thus blocks FDD. The issue is somewhere in the Xilinx code or in the idea to use
    // both XPIO & XDMA going high for this getBridgeStatus().
    if (!isAcsiNotScsi)
    { // only for SCSI interface!
        if (!isBusIdle())
        { // if the bus is not idle, do the reset
            resetBridge();
        }
    }
}

void onButtonPress(void)
{
    uint16_t cnt, diff;

    EXTI->PR = BUTTON; // clear pending EXTI

    cnt = TIM1->CNT; // get the current time
    diff = cnt - prevBtnPressTime;

    if (diff < 500)
    { // the previous button press happened less than 250 ms before? ignore
        return;
    }

    prevBtnPressTime = cnt; // store current time

    currentLed++; // change LED

    if (currentLed > 2)
    { // overflow?
        currentLed = 0xff;
    }

    fixLedsByEnabledImgs(); // if we switched to not enabled floppy image, fix this

    showCurrentLED(); // and show the current LED
}

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
    // if we came here, everything went OK
    if (enabledIDs[id])
    { // for this ID is enabled (to let the CS commands be handled by any CE ID, even the one not assigned to CE SD)
        uint8_t processedLocally;
        processedLocally = tryProcessLocally(); // try to process command locally

        if (processedLocally)
        { // if it was processed locally, quit
            return;
        }
    }

    //----------------
    // if we got here, we should handle this in host (RPi)
    for (i = 0; i < 7; i++)
    { // fill the command to array
        atnSendACSIcommand[4 + i] = (((uint16_t)cmd[i * 2 + 0]) << 8) | cmd[i * 2 + 1];
    }

    timeoutStart(); // start the timeout timer to give the rest of code full timeout time

    sendBufferToHost((uint8_t *)&atnSendACSIcommand[0], ATN_SENDACSICOMMAND_LEN_TX * 2);

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
    if (!enabledIDs[id])
    { // if this ID is not enabled, quit
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
            if (enabledIDs[i])
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
    if (!enabledIDs[id])
    { // if this ID is not enabled, quit
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
    uint16_t i, loopCount, l, dataBytesCount;
    uint8_t dataMarkerFound;
    uint8_t res;
    TReadBuffer *rdBufNow;
    uint16_t *pData;

    seqNo = 0;
    state = STATE_GET_COMMAND; // this will be the next state once this function finishes with fail

    // nothing to send AND should send status? then just quit with status byte
    if (dataCnt == 0 && withStatus)
    {
        PIO_read(statusByte);
        return;
    }

    // calculate how many loops we will have to do
    loopCount = dataCnt / 512;

    if ((dataCnt % 512) != 0)
    {
        loopCount++;
    }

    // receive 0th data block in rdBuf1
    rdBufNow = &rdBuf1;

    startSpiDmaForDataRead(dataCnt, rdBufNow);
    dataCnt -= (uint32_t)rdBufNow->dataBytesCount; // update remaining data size

    // now start the double buffered transfer to ST
    setDataDirection(DIR_SEND); // data direction for reading

    for (l = 0; l < loopCount; l++)
    {
        // first wait until all data arrives in SPI DMA transfer
        while (!spiDmaIsIdle)
        {
            if (timeout())
            {                               // if the data from host doesn't come within timeout, quit
                setDataDirection(DIR_RECV); // data direction for writing, and quit
                return;
            }
        }

        // if after transfering this block there should be another block of data
        if (dataCnt > 0)
        {
            TReadBuffer *nextRdBuffer = rdBufNow->next;

            startSpiDmaForDataRead(dataCnt, nextRdBuffer);     // start receiving data to the other buffer
            dataCnt -= (uint32_t)nextRdBuffer->dataBytesCount; // update remaining data size
        }

        ///////////////////////////////////////////////////////////////
        // send the received data to ST
        // find the data marker
        dataMarkerFound = FALSE;
        pData = &rdBufNow->buffer[0];

        for (i = 0; i < rdBufNow->count; i++)
        {
            uint16_t data;

            data = *pData; // get data
            pData++;

            if (data == CMD_DATA_MARKER)
            { // found data marker?
                dataMarkerFound = TRUE;
                break;
            }
        }

        if (dataMarkerFound == FALSE)
        { // didn't find the data marker?
            if (withStatus)
            {
                PIO_read(SCSI_ST_CHECK_CONDITION); // send status: CHECK CONDITION and quit
            }
            return;
        }

        // now try to trasmit the data
        dataBytesCount = rdBufNow->dataBytesCount;

        res = dataReadCloop(pData, dataBytesCount);

        if (res == 0)
        {
            setDataDirection(DIR_RECV); // data direction for writing, and quit
            return;
        }

        // one cycle finished, now swap buffers and start all over again
        rdBufNow = rdBufNow->next; // swap buffers
    }

    if (withStatus)
    { // if should send status, then send status and go to STATE_GET_COMMAND
        state = STATE_GET_COMMAND;
        PIO_read(statusByte); // send the status to Atari
    }
    else
    { // if shouldn't send status here, switch to state STATE_READ_STATUS, which will retrieve status from RPi and send it to ST
        state = STATE_READ_STATUS;
    }
}

void startSpiDmaForDataRead(uint32_t dataCnt, TReadBuffer *readBfr)
{
    uint32_t subCount, recvCount;

    // calculate how much we need to transfer in 0th data buffer
    subCount = MIN(dataCnt, 512); // uint8_ts to receive
    recvCount = subCount / 2;     // uint16_ts to receive: convert # of uint8_ts to # of uint16_ts
    recvCount += (subCount & 1);  // if subCount is odd number, then we need to transfer 1 uint16_t more
    recvCount += 6;               // receive few words more than just data - 2 uint16_ts start, 1 uint16_t CMD_DATA_MARKER

    readBfr->dataBytesCount = subCount; // store the count of ONLY data
    readBfr->count = recvCount;         // store the count of ALL uint16_ts in buffer, including headers and other markers

    // now transfer the 0th data buffer over SPI
    atnMoreData[4] = seqNo++; // set the sequence # to Attention
    sendBufferToHost((uint8_t *)&atnMoreData[0], 6 * 2);
}

void onDataWrite(void)
{
    uint8_t firstLoop, previousSpiSuccess;
    uint16_t subCount, recvCount;
    uint16_t index, data, i, value;
    TWriteBuffer *wrBufNow;

    seqNo = 0;
    wrBufNow = &wrBuf1;

    setDataDirection(DIR_RECV); // data direction for reading

    firstLoop = TRUE;

    while (dataCnt > 0)
    { // something to write?
        // request maximum 512 bytes from host
        subCount = (dataCnt > 512) ? 512 : dataCnt;
        dataCnt -= subCount;

        wrBufNow->buffer[4] = seqNo; // set the sequence # to Attention
        seqNo++;

        recvCount = subCount / 2;    // uint16_ts to receive: convert # of uint8_ts to # of uint16_ts
        recvCount += (subCount & 1); // if subCount is odd number, then we need to transfer 1 uint16_t more

        index = 5; // length of header before data

        for (i = 0; i < recvCount; i++)
        {                        // write this many uint16_ts
            value = DMA_write(); // get data from Atari
            value = value << 8;  // store as upper byte

            if (brStat == E_TimeOut)
            {                              // if timeout occured
                state = STATE_GET_COMMAND; // transfer failed, don't send status, just get next command
                return;
            }

            subCount--;
            if (subCount == 0)
            {                                    // in case of odd data count
                wrBufNow->buffer[index] = value; // store data
                index++;

                break;
            }

            data = DMA_write();   // get data from Atari
            value = value | data; // store as lower byte

            if (brStat == E_TimeOut)
            {                              // if timeout occured
                state = STATE_GET_COMMAND; // transfer failed, don't send status, just get next command
                return;
            }

            subCount--;

            wrBufNow->buffer[index] = value; // store data
            index++;
        }

        wrBufNow->buffer[index] = 0; // terminating zero
        wrBufNow->count = index + 1; // store count, +1 because we have terminating zero

        //----------
        // set up the SPI DMA transfer
        previousSpiSuccess = sendBufferToHost((uint8_t *)&wrBufNow->buffer[0], wrBufNow->count * 2);

        // if this is not the first loop and the previous SPI transfer failed (something from this WRITE command was not transfered to host), fail
        if (!firstLoop && !previousSpiSuccess)
        {
            state = STATE_GET_COMMAND;
            return;
        }

        firstLoop = FALSE;
        //----------

        wrBufNow = wrBufNow->next; // use next write buffer
    }

    state = STATE_READ_STATUS; // continue with sending the status
}

void onReadStatus(void)
{
    uint8_t i, newStatus;

    newStatus = 0xff; // no status received

    sendBufferToHost(&atnGetStatus[0], ATN_GETSTATUS_LEN_TX * 2);

    spiDma_waitForFinish();

    for (i = 0; i < 8; i++)
    { // go through the received buffer
        if (cmdBuffer[i] == CMD_SEND_STATUS)
        {
            newStatus = cmdBuffer[i + 1] >> 8;
            break;
        }
    }

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

void processHostCommands(void)
{
    uint8_t i, j;

    for (i = 0; i < CMD_BUFFER_LENGTH; i++)
    {
        switch (cmdBuffer[i])
        {
        // process ACSI configuration
        case CMD_ACSI_CONFIG:
            handleAcsiConfig((uint8_t)(cmdBuffer[i + 1] >> 8));

            configWords.acsi = cmdBuffer[i + 1]; // store config words

            cmdBuffer[i] = 0; // clear this command
            cmdBuffer[i + 1] = 0;
            cmdBuffer[i + 2] = 0;

            i += 2;
            break;

        case CMD_DATA_WRITE:
            dataCnt = cmdBuffer[i + 1]; // store the count of bytes we should WRITE - highest and middle byte
            dataCnt = dataCnt << 8;
            dataCnt |= cmdBuffer[i + 2] >> 8; // lowest byte

            statusByte = cmdBuffer[i + 2] & 0xff;

            for (j = 0; j < 3; j++)
            { // clear this command
                cmdBuffer[i + j] = 0;
            }

            state = STATE_DATA_WRITE; // go into DATA_WRITE state
            i += 2;
            break;

        case CMD_DATA_READ_WITH_STATUS:
        case CMD_DATA_READ_WITHOUT_STATUS:
        {
            uint8_t whichCommand = cmdBuffer[i];

            dataCnt = cmdBuffer[i + 1]; // store the count of bytes we should READ - highest and middle byte
            dataCnt = dataCnt << 8;
            dataCnt |= cmdBuffer[i + 2] >> 8; // lowest byte

            statusByte = cmdBuffer[i + 2] & 0xff;

            for (j = 0; j < 3; j++)
            { // clear this command
                cmdBuffer[i + j] = 0;
            }

            if (whichCommand == CMD_DATA_READ_WITH_STATUS)
            { // CMD_DATA_READ? go into STATE_DATA_READ_WITH_STATUS state
                state = STATE_DATA_READ_WITH_STATUS;
            }
            else
            { // CMD_DATA_READ_WITHOUT_STATUS?
                state = STATE_DATA_READ_WITHOUT_STATUS;
            }

            i += 2;
            break;
        }
        }
    }
}

void handleAcsiConfig(uint8_t acsiIds)
{
    int j;

    firstConfigReceived = TRUE; // mark that we've received 1st config

    for (j = 0; j < 8; j++)
    { // for each bit in ids set the flag in enabledIDs[]
        if (acsiIds & (1 << j))
        {
            enabledIDs[j] = TRUE;
        }
        else
        {
            enabledIDs[j] = FALSE;
        }
    }

    // TODO: check if new config different from previous, then write settings to EEPROM
}

void storeHeader(uint8_t *bfr, uint16_t atnCode, uint32_t txLen)
{
    storeDword(bfr, 0xc050d1c5); //  0..3: 0xc050d1c5 [COSmODICS] (4 bytes)
    storeWord(bfr + 4, atnCode); //  4..5: ATN code (2 bytes)
    storeDword(bfr + 6, txLen);  //  6..9: txLen (4 bytes)
}

void setupAtnBuffers(void)
{
    storeHeader(atnSendFwVersion, ATN_FW_VERSION, 0);
    storeWord(atnSendFwVersion + 10, version[0]);
    storeWord(atnSendFwVersion + 12, version[1]);

    storeHeader(atnSendACSIcommand, ATN_ACSI_COMMAND, 0);
    storeHeader(atnMoreData, ATN_READ_MORE_DATA, 0);
    storeHeader(wrBuf1.buffer, ATN_WRITE_MORE_DATA, 0);
    storeHeader(wrBuf2.buffer, ATN_WRITE_MORE_DATA, 0);
    storeHeader(atnGetStatus, ATN_GET_STATUS, 0);

    wrBuf1.next = (void *)&wrBuf2;
    wrBuf2.next = (void *)&wrBuf1;

    rdBuf1.next = (void *)&rdBuf2;
    rdBuf2.next = (void *)&rdBuf1;
}

uint8_t sendBufferToHost(uint8_t *bfr, uint32_t txCount)
{
    storeDword(bfr + 6, txLen); // store the tx length on index 6..9

    // TODO: add sending of data

    return TRUE;
}
