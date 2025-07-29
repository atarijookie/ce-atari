
#include <cstdint>
#include "defs.h"
#include "display.h"
#include "utils.h"

#include <Wire.h>
#include <U8g2lib.h>

U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0,  /* reset=*/ U8X8_PIN_NONE, /* clock=*/ PIN_SCL, /* data=*/ PIN_SDA);

#define SCREEN_WIDTH 128    // OLED display width, in pixels
#define SCREEN_HEIGHT 32    // OLED display height, in pixels

const uint8_t DISPLAY_I2C_ADDRESS = 0x3C; // Common for SSD1306 displays
bool displayPresent = false;

bool isDisplayConnected(uint8_t address)
{
    Wire.beginTransmission(address);
    return (Wire.endTransmission() == 0); // 0 means success
}

void displayInit(void)
{
    Wire.begin(PIN_SDA, PIN_SCL);       // remap i2c to custom pins

    displayPresent = isDisplayConnected(DISPLAY_I2C_ADDRESS);
    if(!displayPresent)
    {
        Serial.println("displayInit - i2c display not connected");
        return;
    }

    u8g2.begin();
    u8g2.clearBuffer();          // clear the internal memory
    u8g2.setFont(u8g2_font_ncenB08_tr); // choose a suitable font

    displayMessage("CE starting");
}

void displayMessage(const char* msg1, const char* msg2, const char* msg3)
{
    if(!displayPresent)
    {
        return;
    }

    u8g2.clearBuffer();          // clear the internal memory

    if(msg1) u8g2.drawStr(0, 10, msg1);
    if(msg2) u8g2.drawStr(0, 21, msg2);
    if(msg3) u8g2.drawStr(0, 32, msg3);

    u8g2.sendBuffer();           // transfer internal memory to the display
}
