#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/spi.h"
#include "hardware/i2c.h"
#include "hardware/dma.h"
#include "hardware/timer.h"
#include "hardware/uart.h"
#include "hardware/watchdog.h"

#include "defs.h"
#include "display.h"
#include "utils.h"
#include "ssd1306.h"
#include "adafruit_gfx.h"
#include "lcdfont.h"

#define SCREEN_WIDTH 128    // OLED display width, in pixels
#define SCREEN_HEIGHT 32    // OLED display height, in pixels

const uint8_t DISPLAY_I2C_ADDRESS = 0x3C; // Common for SSD1306 displays
bool displayPresent = false;

SSD1306* display;
Adafruit_GFX* gfx;

bool isDisplayConnected(uint8_t address)
{
    uint8_t rxdata;
    int ret = i2c_read_blocking(i2c_default, address, &rxdata, 1, false);

    return (ret >= 0);  // -1 on error, zero or positive values mean success
}

void displayInit(void)
{
    // I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT, 400*1000);

    gpio_set_function(PIN_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_SDA);
    gpio_pull_up(PIN_SCL);

    displayPresent = isDisplayConnected(DISPLAY_I2C_ADDRESS);

    if(!displayPresent) {
        printf("displayInit - i2c display not connected\n");
        return;
    }
    printf("displayInit - i2c display found\n");

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
