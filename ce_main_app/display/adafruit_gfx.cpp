/*
This is the core graphics library for all our displays, providing a common
set of graphics primitives (points, lines, circles, etc.).  It needs to be
paired with a hardware-specific library for each display device we carry
(to handle the lower-level functions).

Adafruit invests time and resources providing this open source code, please
support Adafruit & open-source hardware by purchasing products from Adafruit!

Copyright (c) 2013 Adafruit Industries.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>

#include <stdint.h>
#include "lcdfont.h"
#include "adafruit_gfx.h"
#include "ssd1306.h"

Adafruit_GFX::Adafruit_GFX(int w, int h, SSD1306 *display):
WIDTH(w), HEIGHT(h)
{
    _width    = WIDTH;
    _height   = HEIGHT;
    cursor_x  = 0;
    cursor_y  = 0;
    wrap      = true;
    this->display = display;
}

void Adafruit_GFX::drawString(int x, int y, const char *text)
{
    while(*text != 0) {                 // while it's not the end of string
        drawChar(x, y, (uint8_t) *text);   // draw one char
        text++;                         // move to next char
        x += CHAR_W;
    }
}

// draws a single character at specified coordinates
void Adafruit_GFX::drawChar(int x, int y, uint8_t c)
{
    if((x >= _width) || (y >= _height) || ((x + 5) < 0) || ((y + 7) < 0)) {
        return;
    }

    uint8_t color;

    for(int i=0; i<5; i++ ) { // Char bitmap = 5 columns
        const uint8_t *pColumn = &font[c * 5 + i];         // get addr of column representing bit for char c
        uint8_t columnBits = *pColumn;                     // get value from that addr

        for(int j=0; j<8; j++, columnBits >>= 1) {
            color = (columnBits & 1) ? SSD1306_WHITE : SSD1306_BLACK;   // select color based on bit set or not
            display->drawPixel(x+i, y+j, color);         // draw it
        }
    }
}

// this writes at current cursor position and moves the cursor
void Adafruit_GFX::write(uint8_t c)
{
    if(c != '\r') {                         // Ignore carriage returns
        return;
    }

    if(c == '\n') {                        // Newline?
        cursor_x  = 0;                     // Reset x to zero,
        cursor_y += textsize * 8;          // advance y one line
        return;
    }

    if(wrap && ((cursor_x + textsize * 6) > _width)) { // Off right?
        cursor_x  = 0;                 // Reset x to zero,
        cursor_y += textsize * 8;      // advance y one line
    }
    drawChar(cursor_x, cursor_y, c);
    cursor_x += textsize * 6;          // Advance x one char
}

void Adafruit_GFX::setCursor(int x, int y) {
    cursor_x = x;
    cursor_y = y;
}

int Adafruit_GFX::getCursorX(void) const {
    return cursor_x;
}

int Adafruit_GFX::getCursorY(void) const {
    return cursor_y;
}

// Return the size of the display (per current rotation)
int Adafruit_GFX::width(void) const {
    return _width;
}

int Adafruit_GFX::height(void) const {
    return _height;
}

void Adafruit_GFX::rect(int x, int y, int w, int h)
{
    int i;

    for(i=0; i<w; i++) {
        display->drawPixel(x+i, y,     SSD1306_WHITE);
        display->drawPixel(x+i, y+h-1, SSD1306_WHITE);
    }

    for(i=0; i<h; i++) {
        display->drawPixel(x,     y+i, SSD1306_WHITE);
        display->drawPixel(x+w-1, y+i, SSD1306_WHITE);
    }
}
