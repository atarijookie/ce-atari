#include <Ethernet.h>
#include <EEPROM.h>

#include "pico/multicore.h"
#include "pico/sync.h"

#include "defs.h"
#include "bridge.h"
#include "utils.h"
#include "command_handling.h"
#include "connection.h"
#include "display.h"
#include "ikbd.h"
#include "ipc.h"

uint16_t version[2] = {0xa025, 0x1117}; // this means: hAns, 2025-11-17

uint8_t atnSendFwVersion[ATN_SENDFWVERSION_LEN_TX];
uint8_t atnSendACSIcommand[ATN_SENDACSICOMMAND_LEN_TX];

void handleButton(void);

EthernetClient client;

extern volatile bool core1running;
void core1_main_loop(void);

void setup(void)
{
    gpio_set_function(PIN_LED_EVB, GPIO_FUNC_SIO);
    gpio_set_dir(PIN_LED_EVB, GPIO_OUT);
    LED_ON;             // turn LED on during setup

    debugInit();
    debug("\n\n------------\nCORE 0 setup\n");

    loadSettings();
    ipcInit();

    multicore_launch_core1(core1_main_loop);

    int loops = 0;
    while(true) {
        #ifdef LOG_LED
        debugFromQueue();
        #endif

        delay(100);
        loops++;

        if(loops >= 10) {
            loops = 0;
            debug("CORE 0 waiting for CORE 1\n");
        }

        if(core1running) {
            break;
        }
    }

    ikbdInit();

    displayInit();

    setupAtnBuffers(); // fill the ATN buffers with needed headers and terminators

    debug("enabledIDs: %02X\n", settings.enabledIDs);

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

    debug("eth ip: %s\n", Ethernet.localIP().toString().c_str());

    // store mac to fw version buffer
    memcpy(atnSendFwVersion + TX_HEADER_SIZE + 6, settings.mac, 6);

    debug("mac: %02X:%02X:%02X:%02X:%02X:%02X\n", atnSendFwVersion[TX_HEADER_SIZE + 6], atnSendFwVersion[TX_HEADER_SIZE + 7], atnSendFwVersion[TX_HEADER_SIZE + 8],
                                                  atnSendFwVersion[TX_HEADER_SIZE + 9], atnSendFwVersion[TX_HEADER_SIZE + 10], atnSendFwVersion[TX_HEADER_SIZE + 11]);
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
    uint8_t header[TX_HEADER_SIZE];

    LED_OFF;             // turn LED off when reached main loop
    debug("CORE 0 main\n");

    while(1)
    {
        #ifdef LOG_LED
        debugFromQueue();
        #endif

        // discover CE server, connect to CE server
        connectToHost();

        // handle any data incoming
        handleIncommingData();

        // something in the queue for core0? get it, handle it
        IPCbuffer* bfr = ipcGetBufferFromFifo(0);
        if(bfr) {
            // debug("c0: c %d\n", bfr->command);
            switch(bfr->command)
            {
                // report FW version to host
                case STATE_SEND_FW_VER:
                    LED_TOGGLE;
                    sendHeaderAndDataToHost(SOCK_HDD, atnSendFwVersion, ATN_SENDFWVERSION_LEN_TX - TX_HEADER_SIZE);
                    break;

                // send ACSI command to host
                case STATE_GET_COMMAND:
                    memcpy(atnSendACSIcommand + TX_HEADER_SIZE, bfr->data, bfr->length);
                    sendHeaderAndDataToHost(SOCK_HDD, atnSendACSIcommand, ATN_SENDACSICOMMAND_LEN_TX - TX_HEADER_SIZE);
                    break;

                // create and send one header at the start
                case STATE_SEND_WRITE_MORE_DATA:
                    storeHeader(header, ATN_WRITE_MORE_DATA, bfr->length);
                    sendDataToHost(SOCK_HDD, header, TX_HEADER_SIZE);
                    break;

                // should write this data to host
                case STATE_DATA_WRITE:
                    sendDataToHost(SOCK_HDD, bfr->data, bfr->length);
                    break;
            }

            bfr->free = true;
        }

        ikbdProcessing();

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
