#ifndef __BUTTONS_H__
#define __BUTTONS_H__

#include <arduino.h>

#define BTN_PRESS_SHORT     500
#define BTN_PRESS_SAVE      2000
#define BTN_PRESS_CAPTIVE   5000

#define BTN_NONE    0
#define BTN_PREV    1
#define BTN_SELECT  2
#define BTN_NEXT    3

void handleAnalogButtons(void);
void onButtonStateChanged(int buttonState, uint32_t now, uint32_t& buttonPressTime);
void duringButtonPressed(uint32_t now, uint32_t& buttonPressTime);
void handleBootButton(void);
void handleAllButtons(void);

#endif
