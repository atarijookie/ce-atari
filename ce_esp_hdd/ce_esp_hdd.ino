#include <Preferences.h>
#include "WiFi.h"

#include "defs.h"
#include "bridge.h"
#include "utils.h"
#include "command_handling.h"
#include "connection.h"

Preferences preferences;

void onButtonPress(void);

void processHostCommands(void);

uint8_t sendBufferToHost(uint8_t *bfr, uint32_t txCount);

uint16_t version[2] = {0xa025, 0x0430}; // this means: hAns, 2025-04-30

char *VERSION_STRING_SHORT = {"4.00"};
char *DATE_STRING = {"04/30/25"}; // MM/DD/YY

volatile uint8_t sendFwVersion;
uint8_t atnSendFwVersion[ATN_SENDFWVERSION_LEN_TX];
uint8_t atnSendACSIcommand[ATN_SENDACSICOMMAND_LEN_TX];

uint8_t state;
uint32_t dataCnt;
uint8_t statusByte;

uint16_t seqNo = 0;
uint8_t atnMoreData[ATN_READMOREDATA_LEN_TX];

uint8_t atnGetStatus[ATN_GETSTATUS_LEN_TX];

TWriteBuffer wrBuf1, wrBuf2;
TReadBuffer rdBuf1, rdBuf2;
uint16_t smallDataBuffer[2];

uint8_t cmdBuffer[CMD_BUFFER_LENGTH];

//----------
uint8_t *cmd;   // received command bytes, should point beyond the header in atnSendACSIcommand
uint8_t cmdLen;  // length of received command
uint8_t brStat;  // status from bridge
uint8_t lastScsiStatusByte;

uint8_t enabledIDs;

uint8_t isAcsiNotScsi;
uint8_t busIdle;

uint8_t firstConfigReceived; // used to turn LEDs on after first config received
uint8_t shouldProcessCommands;

uint16_t prevBtnPressTime;

uint32_t lastSendFwTime;

uint8_t btnDownTime;

void sendFwToHost(void);

void setup(void)
{
    Serial.begin(115200);       // uart0 for debug strings
    Serial1.begin(19200);       // uart1 for IKBD / eeprom chip

    #define INPUTS_COUNT 12
    int inputs[INPUTS_COUNT] = {PIN_D0, PIN_D1, PIN_D2, PIN_D3, PIN_D4, PIN_D5, PIN_D6, PIN_D7, PIN_CMD1ST, PIN_EOT, PIN_SDA, PIN_BOOT_BTN};

    for (int i = 0; i < INPUTS_COUNT; i++)
    {
        pinMode(inputs[i], INPUT);
    }

    #define OUTPUTS_COUNT 5
    int outputs[OUTPUTS_COUNT] = {PIN_OUT_OE, PIN_FF12D, PIN_INT_TRIG, PIN_DRQ_TRIG, PIN_SCL};

    for (int i = 0; i < OUTPUTS_COUNT; i++)
    {
        pinMode(outputs[i], OUTPUT);
    }

    // read acsi ids
    preferences.begin("acsi", PREFERENCES_RO_MODE);
    enabledIDs = preferences.getUChar("ids", 0); 
    preferences.end();

    cmd = atnSendACSIcommand + TX_HEADER_SIZE;      // place command beyond the header
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
    while(1)
    {
        // connect to wifi, discover CE server, connect to CE server
        connectToHost();

        // get the command from ACSI and send it to host
        // IN  STATE: STATE_GET_COMMAND
        // OUT STATE: WAIT_COMMAND_RESPONSE when GOOD, STATE_GET_COMMAND when FAIL
        if (state == STATE_GET_COMMAND)
        {
            if(PIO_gotFirstCmdByte())       // if 1st CMD byte was received
            {
                onGetCommand();
            }
            else                // in command waiting state, nothing to do and should send FW version?
            {
                uint32_t now = millis();

                if ((now - lastSendFwTime) >= 1000)
                {
                    sendFwToHost();
                }
            }
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
            // at this point it's either success or fail, but we're finished here

            timerSetup_cmdTimeoutChangeLength(CMD_TIMEOUT_SHORT); // after data transfer restore short timeout value
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
            // at this point it's either success or fail, but we're finished here
        }

        // sending and receiving data over SPI using DMA
        // IN  STATE: any
        // OUT STATE: STATE_DATA_WRITE, STATE_DATA_READ_WITH_STATUS, STATE_DATA_READ_WITHOUT_STATUS, or unchanged
        // if (spiDmaIsIdle && shouldProcessCommands)
        // {                          // SPI DMA: nothing to Tx and nothing to Rx?
        //     processHostCommands(); // and process all the received commands

        //     shouldProcessCommands = FALSE; // mark that we don't need to process commands until next time
        // }

        if (timeout())      // if the data from host doesn't come within timeout, quit
        {
            state = STATE_GET_COMMAND;
            
            // if something was wrong, reset XILINX so it won't get stuck
            if (brStat != E_OK)
            {
                resetBridge();
            }

            // The following goes only for SCSI interface, because current getBridgeStatus() (which is called from isBusIdle())
            // triggers INT going low, and thus blocks FDD. The issue is somewhere in the Xilinx code or in the idea to use
            // both XPIO & XDMA going high for this getBridgeStatus().
            if (!isAcsiNotScsi && !isBusIdle())
            { // only for SCSI interface!
                resetBridge();
            }
        }

        //---------------------------
        // // sending and receiving data over SPI using DMA
        // if (shouldProcessCommands)
        // {                          // SPI DMA: nothing to Tx and nothing to Rx?
        //     processHostCommands(); // and process all the received commands
        //     shouldProcessCommands = FALSE; // mark that we don't need to process commands until next time
        // }

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
    sendBufferToHost(atnSendFwVersion, ATN_SENDFWVERSION_LEN_TX);
    shouldProcessCommands = TRUE;
}

void onButtonPress(void)
{
    // uint16_t cnt, diff;

    // EXTI->PR = BUTTON; // clear pending EXTI

    // cnt = TIM1->CNT; // get the current time
    // diff = cnt - prevBtnPressTime;

    // if (diff < 500)
    // { // the previous button press happened less than 250 ms before? ignore
    //     return;
    // }

    // prevBtnPressTime = cnt; // store current time
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
    sendBufferToHost(atnMoreData, ATN_READMOREDATA_LEN_TX);
}

void handleAcsiConfig(uint8_t newAcsiIds)
{
    firstConfigReceived = TRUE; // mark that we've received 1st config

    // check if new config different from previous, then write settings to EEPROM
    if(enabledIDs != newAcsiIds) {
        enabledIDs = newAcsiIds;

        preferences.begin("acsi", PREFERENCES_RW_MODE);
        preferences.putUChar("ids", newAcsiIds); 
        preferences.end();
    }
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
    storeWord(atnSendFwVersion + TX_HEADER_SIZE, version[0]);
    storeWord(atnSendFwVersion + TX_HEADER_SIZE + 2, version[1]);

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

/*
    @param bfr Pointer to start of the data buffer
    @param txCount Size of the data portion after the header (header is TX_HEADER_SIZE bytes big) in bytes
*/
uint8_t sendBufferToHost(uint8_t *bfr, uint32_t txCount)
{
    storeDword(bfr + 6, txCount); // store the tx length on index 6..9

    // TODO: add sending of data - txCount + TX_HEADER_SIZE

    return TRUE;
}

