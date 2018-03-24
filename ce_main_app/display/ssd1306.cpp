/*********************************************************************
This is a library for our Monochrome OLEDs based on SSD1306 drivers

Adafruit invests time and resources providing this open source code,
please support Adafruit and open-source hardware by purchasing
products from Adafruit!

Written by Limor Fried/Ladyada  for Adafruit Industries.
BSD license, check license.txt for more information
All text above, and the splash screen below must be included in any redistribution
*********************************************************************/

#include <stdlib.h>
#include <string.h>

#include "ssd1306.h"
#include "swi2c.h"
#include "../gpio.h"
#include "../utils.h"

BYTE _vccstate;

extern SoftI2CMaster *i2c;

// the memory buffer for the LCD
static BYTE buffer[SSD1306_LCDHEIGHT * BYTES_PER_LINE];

int getRotation(void)
{
    return 0;
}

#define ssd1306_swap(a, b) { WORD t = a; a = b; b = t; }

// the most basic function, set a single pixel
void ssd1306_drawPixel(WORD x, WORD y, WORD color)
{
  if ((x < 0) || (x >= SSD1306_LCDWIDTH) || (y < 0) || (y >= SSD1306_LCDHEIGHT))
    return;

  // check rotation, move pixel around if necessary
  switch (getRotation()) {
  case 1:
    ssd1306_swap(x, y);
    x = SSD1306_LCDWIDTH - x - 1;
    break;
  case 2:
    x = SSD1306_LCDWIDTH - x - 1;
    y = SSD1306_LCDHEIGHT - y - 1;
    break;
  case 3:
    ssd1306_swap(x, y);
    y = SSD1306_LCDHEIGHT - y - 1;
    break;
  }

  // x is which column
    switch (color)
    {
      case WHITE:   buffer[x+ y*BYTES_PER_LINE] |=  (1 << (y&7)); break;
      case BLACK:   buffer[x+ y*BYTES_PER_LINE] &= ~(1 << (y&7)); break;
      case INVERSE: buffer[x+ y*BYTES_PER_LINE] ^=  (1 << (y&7)); break;
    }
}

bool ssd1306_begin(BYTE vccstate) {
    _vccstate = vccstate;

    int res;

  // Init sequence
  res = ssd1306_command(SSD1306_DISPLAYOFF);              // 0xAE

  if(!res)      // if 1st command failed, don't bother with the rest
      return false;

  ssd1306_command(SSD1306_SETDISPLAYCLOCKDIV);            // 0xD5
  ssd1306_command(0x80);                                  // the suggested ratio 0x80

  ssd1306_command(SSD1306_SETMULTIPLEX);                  // 0xA8
  ssd1306_command(SSD1306_LCDHEIGHT - 1);

  ssd1306_command(SSD1306_SETDISPLAYOFFSET);              // 0xD3
  ssd1306_command(0x0);                                   // no offset
  ssd1306_command(SSD1306_SETSTARTLINE | 0x0);            // line #0
  ssd1306_command(SSD1306_CHARGEPUMP);                    // 0x8D

  if (vccstate == SSD1306_EXTERNALVCC) {
    ssd1306_command(0x10);
  } else {
    ssd1306_command(0x14); 
  }
  
  ssd1306_command(SSD1306_MEMORYMODE);                    // 0x20
  ssd1306_command(0x00);                                  // 0x0 act like ks0108
  ssd1306_command(SSD1306_SEGREMAP | 0x1);
  ssd1306_command(SSD1306_COMSCANDEC);

 #if defined SSD1306_128_32
  ssd1306_command(SSD1306_SETCOMPINS);                    // 0xDA
  ssd1306_command(0x02); 
  ssd1306_command(SSD1306_SETCONTRAST);                   // 0x81
  ssd1306_command(0x8F);

#elif defined SSD1306_128_64
  ssd1306_command(SSD1306_SETCOMPINS);                    // 0xDA
  ssd1306_command(0x12);
  ssd1306_command(SSD1306_SETCONTRAST);                   // 0x81
  if (vccstate == SSD1306_EXTERNALVCC)
    { ssd1306_command(0x9F); }
  else
    { ssd1306_command(0xCF); }

#elif defined SSD1306_96_16
  ssd1306_command(SSD1306_SETCOMPINS);                    // 0xDA
  ssd1306_command(0x2);   //ada x12
  ssd1306_command(SSD1306_SETCONTRAST);                   // 0x81
  if (vccstate == SSD1306_EXTERNALVCC)
    { ssd1306_command(0x10); }
  else
    { ssd1306_command(0xAF); }

#endif

  ssd1306_command(SSD1306_SETPRECHARGE);                  // 0xd9
  if (vccstate == SSD1306_EXTERNALVCC)
    { ssd1306_command(0x22); }
  else
    { ssd1306_command(0xF1); }
  ssd1306_command(SSD1306_SETVCOMDETECT);                 // 0xDB
  ssd1306_command(0x40);
  ssd1306_command(SSD1306_DISPLAYALLON_RESUME);           // 0xA4
  ssd1306_command(SSD1306_NORMALDISPLAY);                 // 0xA6

  ssd1306_command(SSD1306_DEACTIVATE_SCROLL);

  ssd1306_command(SSD1306_DISPLAYON);                     // turn on oled panel
  return true;
}


void invertDisplay(BYTE i) {
  if (i) {
    ssd1306_command(SSD1306_INVERTDISPLAY);
  } else {
    ssd1306_command(SSD1306_NORMALDISPLAY);
  }
}

int ssd1306_command(BYTE c) {
    BYTE bfr[2];
    bfr[0] = 0;
    bfr[1] = c;

    int res;

    res = i2c->beginTransmission(SSD1306_I2C_ADDRESS);
    i2c->write(bfr, 2);
    i2c->endTransmission();

    return (res == 0);      // if ACK bit on beginTransmission was 0, this command went OK
}

// startscrollright
// Activate a right handed scroll for rows start through stop
// Hint, the display is 16 rows tall. To scroll the whole display, run:
// display.scrollright(0x00, 0x0F)
void startscrollright(BYTE start, BYTE stop){
  ssd1306_command(SSD1306_RIGHT_HORIZONTAL_SCROLL);
  ssd1306_command(0X00);
  ssd1306_command(start);
  ssd1306_command(0X00);
  ssd1306_command(stop);
  ssd1306_command(0X00);
  ssd1306_command(0XFF);
  ssd1306_command(SSD1306_ACTIVATE_SCROLL);
}

// startscrollleft
// Activate a right handed scroll for rows start through stop
// Hint, the display is 16 rows tall. To scroll the whole display, run:
// display.scrollright(0x00, 0x0F)
void startscrollleft(BYTE start, BYTE stop){
  ssd1306_command(SSD1306_LEFT_HORIZONTAL_SCROLL);
  ssd1306_command(0X00);
  ssd1306_command(start);
  ssd1306_command(0X00);
  ssd1306_command(stop);
  ssd1306_command(0X00);
  ssd1306_command(0XFF);
  ssd1306_command(SSD1306_ACTIVATE_SCROLL);
}

// startscrolldiagright
// Activate a diagonal scroll for rows start through stop
// Hint, the display is 16 rows tall. To scroll the whole display, run:
// display.scrollright(0x00, 0x0F)
void startscrolldiagright(BYTE start, BYTE stop){
  ssd1306_command(SSD1306_SET_VERTICAL_SCROLL_AREA);
  ssd1306_command(0X00);
  ssd1306_command(SSD1306_LCDHEIGHT);
  ssd1306_command(SSD1306_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL);
  ssd1306_command(0X00);
  ssd1306_command(start);
  ssd1306_command(0X00);
  ssd1306_command(stop);
  ssd1306_command(0X01);
  ssd1306_command(SSD1306_ACTIVATE_SCROLL);
}

// startscrolldiagleft
// Activate a diagonal scroll for rows start through stop
// Hint, the display is 16 rows tall. To scroll the whole display, run:
// display.scrollright(0x00, 0x0F)
void startscrolldiagleft(BYTE start, BYTE stop){
  ssd1306_command(SSD1306_SET_VERTICAL_SCROLL_AREA);
  ssd1306_command(0X00);
  ssd1306_command(SSD1306_LCDHEIGHT);
  ssd1306_command(SSD1306_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL);
  ssd1306_command(0X00);
  ssd1306_command(start);
  ssd1306_command(0X00);
  ssd1306_command(stop);
  ssd1306_command(0X01);
  ssd1306_command(SSD1306_ACTIVATE_SCROLL);
}

void stopscroll(void){
  ssd1306_command(SSD1306_DEACTIVATE_SCROLL);
}

// Dim the display
// dim = true: display is dimmed
// dim = false: display is normal
void dim(int dim) {
  BYTE contrast;

  if (dim) {
    contrast = 0; // Dimmed display
  } else {
    if (_vccstate == SSD1306_EXTERNALVCC) {
      contrast = 0x9F;
    } else {
      contrast = 0xCF;
    }
  }
  // the range of contrast to too small to be really useful
  // it is useful to dim the display
  ssd1306_command(SSD1306_SETCONTRAST);
  ssd1306_command(contrast);
}

void ssd1306_display(void)
{
    ssd1306_command(SSD1306_COLUMNADDR);
    ssd1306_command(0);   // Column start address (0 = reset)
    ssd1306_command(SSD1306_LCDWIDTH-1); // Column end address (127 = reset)

    ssd1306_command(SSD1306_PAGEADDR);
    ssd1306_command(0); // Page start address (0 = reset)

    // page end: 7 for 64px height, 3 for 32px height, 1 for 16px height
    ssd1306_command(3); // Page end address

    int y;
    for (y=0; y<SSD1306_LCDHEIGHT; y++) {
        BYTE bfr[1 + BYTES_PER_LINE];
        bfr[0] = 0x40;
        memcpy(bfr + 1, buffer + (y * BYTES_PER_LINE), BYTES_PER_LINE);

        i2c->beginTransmission(SSD1306_I2C_ADDRESS);
        i2c->write(bfr, 1 + BYTES_PER_LINE);
        i2c->endTransmission();
    }
}

void ssd1306_clearDisplay(void) {
    memset(buffer, 0, BYTES_PER_LINE * SSD1306_LCDHEIGHT);
}
