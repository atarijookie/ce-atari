/*********************************************************************
This is a library for our Monochrome OLEDs based on SSD1306 drivers

Adafruit invests time and resources providing this open source code,
please support Adafruit and open-source hardware by purchasing
products from Adafruit!

Written by Limor Fried/Ladyada  for Adafruit Industries.
BSD license, check license.txt for more information
*********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ssd1306.h"
#include "i2c2.h"
#include "../utils.h"
#include "../chipinterface.h"

extern ChipInterface* chipInterface;

SSD1306::SSD1306()
{
    buffer = new uint8_t[SSD1306_BUFFER_SIZE];

    i2c = (chipInterface && chipInterface->handlesDisplay()) ? new i2c2() : NULL;
}

SSD1306::~SSD1306()
{
    if(i2c) {
        delete i2c;
        i2c = NULL;
    }

    delete []buffer;
    buffer = NULL;
}

// the most basic function, set a single pixel
void SSD1306::drawPixel(uint16_t x, uint16_t y, uint16_t color)
{
    if ((x < 0) || (x >= SSD1306_LCDWIDTH) || (y < 0) || (y >= SSD1306_LCDHEIGHT))
        return;

    uint16_t idx = x + (y/8) * SSD1306_LCDWIDTH;

    if(idx >= SSD1306_BUFFER_SIZE) {
        return;
    }

    // x is which column
    switch (color)
    {
        case SSD1306_WHITE:   buffer[idx] |=  (1 << (y&7)); break;
        case SSD1306_BLACK:   buffer[idx] &= ~(1 << (y&7)); break;
        case SSD1306_INVERSE: buffer[idx] ^=  (1 << (y&7)); break;
    }
}

bool SSD1306::begin(uint8_t vccstate) {
    this->vccstate = vccstate;

    int res;

    // Init sequence
    res = command(SSD1306_DISPLAYOFF);              // 0xAE

    if(!res)      // if 1st command failed, don't bother with the rest
        return false;

    command(SSD1306_SETDISPLAYCLOCKDIV);            // 0xD5
    command(0x80);                                  // the suggested ratio 0x80

    command(SSD1306_SETMULTIPLEX);                  // 0xA8
    command(SSD1306_LCDHEIGHT - 1);

    command(SSD1306_SETDISPLAYOFFSET);              // 0xD3
    command(0x0);                                   // no offset
    command(SSD1306_SETSTARTLINE | 0x0);            // line #0
    command(SSD1306_CHARGEPUMP);                    // 0x8D

    command(vccstate == SSD1306_EXTERNALVCC ? 0x10 : 0x14);

    command(SSD1306_MEMORYMODE);                    // 0x20
    command(0x00);                                  // 0x0 act like ks0108
    command(SSD1306_SEGREMAP | 0x1);
    command(SSD1306_COMSCANDEC);

    command(SSD1306_SETCOMPINS);                    // 0xDA
    command(0x02);
    command(SSD1306_SETCONTRAST);                   // 0x81
    command(0x8F);

    command(SSD1306_SETPRECHARGE);                  // 0xd9
    command(vccstate == SSD1306_EXTERNALVCC ? 0x22 : 0xF1);

    command(SSD1306_SETVCOMDETECT);                 // 0xDB
    command(0x40);
    command(SSD1306_DISPLAYALLON_RESUME);           // 0xA4
    command(SSD1306_NORMALDISPLAY);                 // 0xA6

    command(SSD1306_DEACTIVATE_SCROLL);

    command(SSD1306_DISPLAYON);                     // turn on oled panel
    return true;
}


void SSD1306::invertDisplay(uint8_t i) {
    command(i ? SSD1306_INVERTDISPLAY : SSD1306_NORMALDISPLAY);
}

int SSD1306::command(uint8_t c) {
    int res = 0;

    if(i2c) {
        res = i2c->i2c_smbus_write_byte_data(SSD1306_I2C_ADDRESS, 0, c);
    }

    return (res == 0);      // if ACK bit on beginTransmission was 0, this command went OK
}

// startscrollright
// Activate a right handed scroll for rows start through stop
// Hint, the display is 16 rows tall. To scroll the whole display, run:
// display.scrollright(0x00, 0x0F)
void SSD1306::startscrollright(uint8_t start, uint8_t stop){
  command(SSD1306_RIGHT_HORIZONTAL_SCROLL);
  command(0X00);
  command(start);
  command(0X00);
  command(stop);
  command(0X00);
  command(0XFF);
  command(SSD1306_ACTIVATE_SCROLL);
}

// startscrollleft
// Activate a right handed scroll for rows start through stop
// Hint, the display is 16 rows tall. To scroll the whole display, run:
// display.scrollright(0x00, 0x0F)
void SSD1306::startscrollleft(uint8_t start, uint8_t stop){
  command(SSD1306_LEFT_HORIZONTAL_SCROLL);
  command(0X00);
  command(start);
  command(0X00);
  command(stop);
  command(0X00);
  command(0XFF);
  command(SSD1306_ACTIVATE_SCROLL);
}

// startscrolldiagright
// Activate a diagonal scroll for rows start through stop
// Hint, the display is 16 rows tall. To scroll the whole display, run:
// display.scrollright(0x00, 0x0F)
void SSD1306::startscrolldiagright(uint8_t start, uint8_t stop){
  command(SSD1306_SET_VERTICAL_SCROLL_AREA);
  command(0X00);
  command(SSD1306_LCDHEIGHT);
  command(SSD1306_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL);
  command(0X00);
  command(start);
  command(0X00);
  command(stop);
  command(0X01);
  command(SSD1306_ACTIVATE_SCROLL);
}

// startscrolldiagleft
// Activate a diagonal scroll for rows start through stop
// Hint, the display is 16 rows tall. To scroll the whole display, run:
// display.scrollright(0x00, 0x0F)
void SSD1306::startscrolldiagleft(uint8_t start, uint8_t stop){
  command(SSD1306_SET_VERTICAL_SCROLL_AREA);
  command(0X00);
  command(SSD1306_LCDHEIGHT);
  command(SSD1306_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL);
  command(0X00);
  command(start);
  command(0X00);
  command(stop);
  command(0X01);
  command(SSD1306_ACTIVATE_SCROLL);
}

void SSD1306::stopscroll(void){
  command(SSD1306_DEACTIVATE_SCROLL);
}

// Dim the display
// dim = true: display is dimmed
// dim = false: display is normal
void SSD1306::dim(int dim) {
    uint8_t contrast;

    if (dim) {
        contrast = 0; // Dimmed display
    } else {
        contrast = (vccstate == SSD1306_EXTERNALVCC) ? 0x9F : 0xCF;
    }
  // the range of contrast to too small to be really useful
  // it is useful to dim the display
  command(SSD1306_SETCONTRAST);
  command(contrast);
}

void SSD1306::display(void)
{
    command(SSD1306_COLUMNADDR);
    command(0);   // Column start address (0 = reset)
    command(SSD1306_LCDWIDTH-1); // Column end address (127 = reset)

    command(SSD1306_PAGEADDR);
    command(0); // Page start address (0 = reset)

    // page end: 7 for 64px height, 3 for 32px height, 1 for 16px height
    command(3); // Page end address

    chipInterface->displayBuffer(buffer, SSD1306_BUFFER_SIZE);  // tell any remote display that this should be displayed

    int y;
    for (y=0; y<SSD1306_LCDHEIGHT; y++) {
        uint8_t *data = buffer + (y * SSD1306_BYTES_PER_LINE);

        if(i2c) {
            i2c->i2c_smbus_write_i2c_block_data(SSD1306_I2C_ADDRESS, 0x40, SSD1306_BYTES_PER_LINE, data);
        }
    }
}

void SSD1306::clearDisplay(void) {
    memset(buffer, 0, SSD1306_BUFFER_SIZE);
}
