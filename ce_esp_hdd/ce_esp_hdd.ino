#include "WiFi.h"
#include <Preferences.h>

#include "defs.h"
#include "bridge.h"
#include "utils.h"
#include "command_handling.h"
#include "connection.h"
#include "captive_portal.h"

Preferences preferences;

uint16_t version[2] = {0xa025, 0x0430}; // this means: hAns, 2025-04-30

uint8_t atnSendFwVersion[ATN_SENDFWVERSION_LEN_TX];
uint8_t atnSendACSIcommand[ATN_SENDACSICOMMAND_LEN_TX];

uint8_t state;
uint32_t dataCnt;
uint8_t statusByte;
bool dataReceived;

uint8_t *cmd;   // received command bytes, should point beyond the header in atnSendACSIcommand
uint8_t cmdLen;  // length of received command
uint8_t brStat;  // status from bridge
uint8_t lastScsiStatusByte;

uint8_t enabledIDs;

uint8_t isAcsiNotScsi;
uint8_t busIdle;

void handleButton(void);

void setup(void)
{
    Serial.begin(115200);       // uart0 for debug strings
    Serial1.begin(19200);       // uart1 for IKBD / eeprom chip

    Serial.println("setup() starting");

    #define INPUTS_COUNT 12
    int inputs[INPUTS_COUNT] = {PIN_D0, PIN_D1, PIN_D2, PIN_D3, PIN_D4, PIN_D5, PIN_D6, PIN_D7, PIN_CMD1ST, PIN_EOT, PIN_SDA, PIN_BOOT_BTN};

    for (int i = 0; i < INPUTS_COUNT; i++)
    {
        pinMode(inputs[i], INPUT);
    }

    pinMode(PIN_BOOT_BTN, INPUT_PULLUP);

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

    setupAtnBuffers(); // fill the ATN buffers with needed headers and terminators

    getBridgeStatus();
    resetBridge();

    Serial.print("setup() done, enabledIDs: ");
    Serial.print(enabledIDs, HEX);
    Serial.println("");

    timeoutClear();
}

void setupAtnBuffers(void)
{
    storeHeader(atnSendACSIcommand, ATN_ACSI_COMMAND, 0);

    memset(atnSendFwVersion, 0, ATN_SENDFWVERSION_LEN_TX);
    storeHeader(atnSendFwVersion, ATN_FW_VERSION, 0);
    storeWord(atnSendFwVersion + TX_HEADER_SIZE, version[0]);
    storeWord(atnSendFwVersion + TX_HEADER_SIZE + 2, version[1]);
    atnSendFwVersion[TX_HEADER_SIZE + 5] = 0x41;                    // v.4, ACSI
}

void loop(void)
{
    uint32_t lastSendFwTime = millis();

    while(1)
    {
        // connect to wifi, discover CE server, connect to CE server
        connectToHost();

        // handle any data incoming
        handleIncommingData();

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
                    lastSendFwTime = now;
                    sendHeaderAndDataToHost(SOCK_HDD, atnSendFwVersion, ATN_SENDFWVERSION_LEN_TX - TX_HEADER_SIZE);
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

            cmdTimeoutChangeLength(CMD_TIMEOUT_SHORT); // after data transfer restore short timeout value
        }

        // transfer the data - write (from ST)
        // IN  STATE: STATE_DATA_WRITE
        // OUT STATE: STATE_READ_STATUS on success, STATE_GET_COMMAND on FAIL
        if (state == STATE_DATA_WRITE)
        {
            longTimeout_basedOnSectorCount(dataCnt >> 9); // set timeout time based on how many sectors are transfered

            onDataWrite();

            cmdTimeoutChangeLength(CMD_TIMEOUT_SHORT); // after data transfer restore short timeout value
        }

        // after write, we will wait for STATUS arrival from host
        // IN  STATE: STATE_WAIT_FOR_STATUS_ARRIVAL
        // OUT STATE: STATE_READ_STATUS

        // this happens after WRITE - wait for status byte, send it to ST (read)
        // IN  STATE: STATE_READ_STATUS
        // OUT STATE: always STATE_GET_COMMAND
        if (state == STATE_READ_STATUS)
        {
            timeoutStart(); // start the timeout timer to give the rest of code full timeout time

            onReadStatus();
            state = STATE_GET_COMMAND; // get the next command
            // at this point it's either success or fail, but we're finished here
        }

        // sending and receiving data over SPI using DMA
        // IN  STATE: any
        // OUT STATE: STATE_DATA_WRITE, STATE_DATA_READ_WITH_STATUS, STATE_DATA_READ_WITHOUT_STATUS, or unchanged

        if (timeout())      // if the data from host doesn't come within timeout, quit
        {
            timeoutClear();
            Serial.println("timeout!");

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
        // check the button state and press duration
        handleButton();
    }
}

void handleButton(void)
{
    static uint32_t lastCheck = millis();
    static int lastButtonState = HIGH;
    static uint32_t buttonPressTime = 0;

    uint32_t now = millis();

    if(now - lastCheck < 100)      // check for button change only every now and then
    {
        return;
    }
    lastCheck = now;

    int buttonState = digitalRead(PIN_BOOT_BTN);    // read button

    if(lastButtonState == buttonState)      // no change in button state? quit
    {
        return;
    }
    lastButtonState = buttonState;

    if(buttonState == LOW)  // button now low, so button pressed
    {
        buttonPressTime = now;
    }
    else                    // button now high, so button released
    {
        uint32_t pressDuration = now - buttonPressTime;

        if(pressDuration < 500)     // on short press
        {

        }

        if(pressDuration > 5000)    // on long press, run captive portal
        {
            runCaptivePortal();
        }
    }    
}
