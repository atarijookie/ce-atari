#include <Ethernet.h>
#include <EEPROM.h>

#include "defs.h"
#include "bridge.h"
#include "utils.h"
#include "command_handling.h"
#include "connection.h"
#include "rw_tasks.h"
#include "display.h"
#include "ikbd.h"

uint16_t version[2] = {0xa025, 0x1117}; // this means: hAns, 2025-11-17

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

uint8_t isAcsiNotScsi;
uint8_t busIdle;

void handleButton(void);

EthernetClient client;

void setup(void)
{
    Serial1.begin(115200);   // uart0 for debug strings

    // Serial1.begin(7812, SERIAL_8N1, /* rxd pin */ PIN_KEYB_TX_ORIG, /* txd pin */ PIN_KEYB_TX); // uart1 for IKBD
    // Serial2.begin(7812, SERIAL_8N1, /* rxd pin */ PIN_KEYB_RX, /* txd pin */ PIN_TXD2);         // uart2 for IKBD

    debug("setup() starting\n");

    loadSettings();

    #define INPUTS_COUNT 13
    int inputs[INPUTS_COUNT] = {PIN_D0, PIN_D1, PIN_D2, PIN_D3, PIN_D4, PIN_D5, PIN_D6, PIN_D7, PIN_CS, PIN_A1, PIN_ACK, PIN_RESET, PIN_SDA};

    for (int i = 0; i < INPUTS_COUNT; i++)
    {
        pinMode(inputs[i], INPUT);
    }

    #define OUTPUTS_COUNT 4
    int outputs[OUTPUTS_COUNT] = {PIN_DATA_DIR, PIN_INT, PIN_DRQ, PIN_SCL};

    for (int i = 0; i < OUTPUTS_COUNT; i++)
    {
        pinMode(outputs[i], OUTPUT);
    }

    cmd = atnSendACSIcommand + TX_HEADER_SIZE;      // place command beyond the header
    state = STATE_GET_COMMAND;

    setupAtnBuffers(); // fill the ATN buffers with needed headers and terminators

    getBridgeStatus();
    resetBridge();

    debug("setup() done, enabledIDs: %02X\n", settings.enabledIDs);

    Ethernet.init(17);              // WIZnet W6100-EVB-Pico
    Ethernet.begin(settings.mac);   // set mac, get IP via hdcp

    if(Ethernet.hardwareStatus() == EthernetNoHardware) {
        debug("No ethernet. HALT!\n");
        while(1);
    }

    while(Ethernet.linkStatus() == LinkOFF) {
        debug("cable not connected\n");
        delay(1000);
    }

    // store mac to fw version buffer
    memcpy(atnSendFwVersion + TX_HEADER_SIZE + 6, settings.mac, 6);

    debug("mac: %02X:%02X:%02X:%02X:%02X:%02X\n", atnSendFwVersion[TX_HEADER_SIZE + 6], atnSendFwVersion[TX_HEADER_SIZE + 7], atnSendFwVersion[TX_HEADER_SIZE + 8],
                                                  atnSendFwVersion[TX_HEADER_SIZE + 9], atnSendFwVersion[TX_HEADER_SIZE + 10], atnSendFwVersion[TX_HEADER_SIZE + 11]);

#ifdef RW_TASKS
    createTasks();      // create the read / write tasks
#endif

    createIkbdTask();   // this task sends ikdb data to host and back

    displayInit();
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
    uint32_t lastYield = millis();

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
                state = onGetCommand();
            }
            else                // in command waiting state, nothing to do and should send FW version?
            {
                uint32_t now = millis();

                if ((now - lastSendFwTime) >= 1000)
                {
#ifdef LOG_MORE
                    // dumpPinStates();        // instead of message about fw, dump pin states
#endif
                    lastSendFwTime = now;
                    sendHeaderAndDataToHost(SOCK_HDD, atnSendFwVersion, ATN_SENDFWVERSION_LEN_TX - TX_HEADER_SIZE);
                }
            }
        }

        // transfer the data - read (to ST)
        // IN  STATE: STATE_DATA_READ_WITH_STATUS or STATE_DATA_READ_WITHOUT_STATUS
        // OUT STATE: STATE_READ_STATUS or STATE_GET_COMMAND
        if (state == STATE_DATA_READ_WITH_STATUS || state == STATE_DATA_READ_WITHOUT_STATUS)
        {
            longTimeout_basedOnSectorCount(dataCnt >> 9); // set timeout time based on how many sectors are transfered

            bool withStatus = state == STATE_DATA_READ_WITH_STATUS;
            state = onDataRead(withStatus);     // read data to Atari

            // if going to get command state, clear timeout, for other states use short timeout
            (state == STATE_GET_COMMAND) ? timeoutClear() : cmdTimeoutChangeLength(CMD_TIMEOUT_SHORT);
        }

        // transfer the data - write (from ST)
        // IN  STATE: STATE_DATA_WRITE
        // OUT STATE: STATE_READ_STATUS on success, STATE_GET_COMMAND on FAIL
        if (state == STATE_DATA_WRITE)
        {
            longTimeout_basedOnSectorCount(dataCnt >> 9); // set timeout time based on how many sectors are transfered

            state = onDataWrite();

            // if going to get command state, clear timeout, for other states use short timeout
            (state == STATE_GET_COMMAND) ? timeoutClear() : cmdTimeoutChangeLength(CMD_TIMEOUT_SHORT);
        }

        // after write, we will wait for STATUS arrival from host
        // IN  STATE: STATE_WAIT_FOR_STATUS_ARRIVAL
        // OUT STATE: STATE_READ_STATUS
        // { no code needed here }

        // this happens after READ - wait for status byte, send it to ST (read)
        // IN  STATE: STATE_READ_STATUS
        // OUT STATE: STATE_GET_COMMAND
        if (state == STATE_READ_STATUS)
        {
            timeoutStart(); // start the timeout timer to give the rest of code full timeout time

            onReadStatus();

            state = STATE_GET_COMMAND;  // get the next command
            timeoutClear();             // clear timeout, no need for it
        }

        // if the data from host doesn't come within timeout, quit
        if (hasTimedOut)
        {
            timeoutClear();

#ifdef LOG_MORE
            debug("State : %d, cmd: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X, timeout at: %d\n",
                state, cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5], cmd[6], cmd[7], cmd[8], cmd[9], cmd[10], cmd[11], millis());
#endif
            state = STATE_GET_COMMAND;

            if (!isBusIdle())
            {
#ifdef LOG_MORE
                debug("resetBridge!\n");
#endif
                resetBridge();
            }
        }

        //---------------------------
        // check the button state and press duration
        handleButton();
    }
}

#define BTN_PRESS_SHORT     500
#define BTN_PRESS_SAVE      2000

// This gets called on button pressed (current button state LOW) or released (current button state HIGH)
void onButtonStateChanged(int buttonState, uint32_t now, uint32_t& buttonPressTime)
{
    // button state change to low, so button just pressed - store time, nothing more to do
    if(buttonState == LOW)
    {
        buttonPressTime = now;
        return;
    }

    //-------
    // button state change to high, so button released
    uint32_t pressDuration = now - buttonPressTime;

    if(pressDuration < BTN_PRESS_SHORT)     // on short press, ikbd enable / disable
    {
        settings.ikbdEnabled = !settings.ikbdEnabled;
    }

    // on longer press, save ikbd enabled flag
    if(pressDuration >= BTN_PRESS_SAVE)
    {
        saveSettings();
    }

    showRunningStateOnDisplay();
}

// Gets called during the button is pressed down, used to show stuff on display for long press.
void duringButtonPressed(uint32_t now, uint32_t& buttonPressTime)
{
    uint32_t pressDuration = now - buttonPressTime;

    // press too short? nothing to show on display
    if(pressDuration < BTN_PRESS_SAVE)
    {
        return;
    }

    // longer press? ask about saving ikbd settings
    if(pressDuration >= BTN_PRESS_SAVE)
    {
        displayMessage(NULL, "Store IKDB enabled?", NULL);
    }

    // longest press? ask about running captive portal
    // if(pressDuration >= BTN_PRESS_CAPTIVE)
    // {
    //     displayMessage(NULL, "Run captive portal?", NULL);
    // }
}

// Check the button pressed / released state, check if button has been just pressed, released,
// or is being held down. Show stuff on display, handle button actions.
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

    // int buttonState = digitalRead(PIN_BOOT_BTN);    // read button

    // bool buttonStateChanged = (lastButtonState != buttonState);
    // lastButtonState = buttonState;

    // if(buttonStateChanged)      // button state changed? (e.g. pressed, released)
    // {
    //     onButtonStateChanged(buttonState, now, buttonPressTime);
    // }
    // else        // button state not changed (stayed released, stayed pressed)
    // {
    //     if(buttonState == LOW)
    //     {
    //         duringButtonPressed(now, buttonPressTime);
    //     }
    // }
}
