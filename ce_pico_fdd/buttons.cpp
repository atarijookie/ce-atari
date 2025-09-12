#include "defs.h"
#include "connection.h"
#include "utils.h"
#include "captive_portal.h"
#include "display.h"
#include "buttons.h"

extern volatile bool ikbdEnabled;   // if true, should send data to host; otherwise just loopback ikdb data back

void handleAnalogButtons(uint32_t now)
{
    static uint8_t prevWhichButton = BTN_NONE;

    // uint16_t val = analogRead(PIN_ANALOG_BTNS);
    uint16_t val = 5000;
    uint8_t whichButton = BTN_NONE;

    // convert analog value to specific button
    if(val < 500) {
        whichButton = BTN_PREV;
    } else if(val < 1700) {
        whichButton = BTN_SELECT;
    } else if(val < 3100) {
        whichButton = BTN_NEXT;
    }

    // button state not changed? nothing to do
    if(prevWhichButton == whichButton) {
        return;
    }
    prevWhichButton = whichButton;

    // no need to handle no-button-release
    if(whichButton = BTN_NONE) {
        return;
    }

    // TODO: handle on button released
    printf("analog button: %d\n", whichButton);
}

// This gets called on button pressed (current button state 0) or released (current button state HIGH)
void onButtonStateChanged(int buttonState, uint32_t now, uint32_t& buttonPressTime)
{
    // button state change to low, so button just pressed - store time, nothing more to do
    if(buttonState == 0)
    {
        buttonPressTime = now;
        return;
    }

    //-------
    // button state change to high, so button released
    uint32_t pressDuration = now - buttonPressTime;

    if(pressDuration < BTN_PRESS_SHORT)     // on short press, ikbd enable / disable
    {
        ikbdEnabled = !ikbdEnabled;
    }

    // on longer press, save ikbd enabled flag
    if(pressDuration >= BTN_PRESS_SAVE && pressDuration < BTN_PRESS_CAPTIVE)
    {
        storeIkbdEnabled(ikbdEnabled);
    }

    // on longest press, run captive portal
    if(pressDuration >= BTN_PRESS_CAPTIVE)
    {
        runCaptivePortal();
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
    if(pressDuration >= BTN_PRESS_SAVE && pressDuration < BTN_PRESS_CAPTIVE)
    {
        displayMessage(NULL, "Store IKDB enabled?", NULL);
    }

    // longest press? ask about running captive portal
    if(pressDuration >= BTN_PRESS_CAPTIVE)
    {
        displayMessage(NULL, "Run captive portal?", NULL);
    }
}

// handle the states of the button connected to GPIO0 (boot pin)
void handleBootButton(uint32_t now)
{
    static int lastButtonState = 1;
    static uint32_t buttonPressTime = 0;

    int buttonState = 1;     // TODO:
    // int buttonState = digitalRead(PIN_BOOT_BTN);    // read button

    bool buttonStateChanged = (lastButtonState != buttonState);
    lastButtonState = buttonState;

    if(buttonStateChanged)      // button state changed? (e.g. pressed, released)
    {
        onButtonStateChanged(buttonState, now, buttonPressTime);
    }
    else        // button state not changed (stayed released, stayed pressed)
    {
        if(buttonState == 0)
        {
            duringButtonPressed(now, buttonPressTime);
        }
    }
}

// Check the button pressed / released state, check if button has been just pressed, released, 
// or is being held down. Show stuff on display, handle button actions.
void handleAllButtons(void)
{
    static uint32_t lastCheck = millis();
    uint32_t now = millis();

    if(now - lastCheck < 100)      // check for button change only every now and then
    {
        return;
    }
    lastCheck = now;

    // handleBootButton(now);
    // TODO: uncomment when there's at least a pull up on analog buttons pin
    //handleAnalogButtons(now);
}
