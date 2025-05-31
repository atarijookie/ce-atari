
#include <cstdint>
#include "defs.h"
#include "display.h"
#include "utils.h"

#include <U8g2lib.h>
U8G2_SSD1306_128X32_UNIVISION_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ PIN_SCL, /* data=*/ PIN_SDA, /* reset=*/ U8X8_PIN_NONE);

#define SCREEN_WIDTH 128    // OLED display width, in pixels
#define SCREEN_HEIGHT 32    // OLED display height, in pixels

void displayInit(void)
{
    u8g2.begin();
    u8g2.clearBuffer();          // clear the internal memory
    u8g2.setFont(u8g2_font_ncenB08_tr); // choose a suitable font

    displayMessage("Starting...");
}

void displayMessage(const char* msg)
{
#ifdef LOG_MORE
    Serial.print("displayMessage: ");
    Serial.println(msg);
#endif

    u8g2.clearBuffer();          // clear the internal memory
    u8g2.drawStr(0, 12, msg);
    u8g2.sendBuffer();           // transfer internal memory to the display
}
