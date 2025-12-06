
#include <cstdint>
#include <Wire.h>

#include "defs.h"
#include "display.h"
#include "utils.h"

#include "hardware/i2c.h"
#include "ssd1306.h"
#include "adafruit_gfx.h"
#include "lcdfont.h"

SSD1306* display;
Adafruit_GFX* gfx;

#define SCREEN_WIDTH 128    // OLED display width, in pixels
#define SCREEN_HEIGHT 32    // OLED display height, in pixels

bool displayPresent = false;

bool isDisplayConnected(uint8_t address)
{
    uint8_t rxdata;
    int ret = i2c_read_timeout_us(DISPLAY_I2C_IFACE, address, &rxdata, 1, false, 100000);

    return (ret >= 0);  // -1 on error, zero or positive values mean success
}

void displayInit(void)
{
    debug("displayInit\n");

    // I2C Initialisation. Using it at 400Khz.
    i2c_init(i2c1, 400000);
    gpio_set_function(PIN_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_SDA);
    gpio_pull_up(PIN_SCL);

    displayPresent = isDisplayConnected(DISPLAY_I2C_ADDRESS);

    if(!displayPresent) {
        debug("displayInit - i2c display not connected\n");
        return;
    }
    debug("displayInit - i2c display found\n");

    display = new SSD1306();

    bool res = display->begin(SSD1306_SWITCHCAPVCC);    // low level OLED library
    display->clearDisplay();
    display->display();

    gfx = new Adafruit_GFX(SSD1306_LCDWIDTH, SSD1306_LCDHEIGHT, display);    // font displaying library

    displayMessage("CE starting");
}

void displayMessage(const char* msg1, const char* msg2, const char* msg3)
{
    if(!displayPresent) {
        return;
    }

    display->clearDisplay();

    if(msg1) gfx->drawString(0,   CHAR_H, msg1);
    if(msg2) gfx->drawString(0, 2*CHAR_H, msg2);
    if(msg3) gfx->drawString(0, 3*CHAR_H, msg3);

    display->display();
}
