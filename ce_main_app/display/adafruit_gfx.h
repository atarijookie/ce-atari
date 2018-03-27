#ifndef _ADAFRUIT_GFX_H
#define _ADAFRUIT_GFX_H

#include "ssd1306.h"

class Adafruit_GFX {

 public:

  Adafruit_GFX(int w, int h, SSD1306 *display); // Constructor

  void setCursor(int x, int y);

  void drawString(int x, int y, const char *text);
  void write(BYTE c);
  void drawChar(int x, int y, BYTE c);

  int height(void) const;
  int width(void) const;

  unsigned int getRotation(void) const;

  // get current cursor position (get rotation safe maximum values, using: width() for x, height() for y)
  int getCursorX(void) const;
  int getCursorY(void) const;

  void rect(int x, int y, int w, int h);

 protected:
  const int WIDTH, HEIGHT;   // This is the 'raw' display w/h - never changes
  int _width, _height;      // Display w/h as modified by current rotation
  int cursor_x, cursor_y;
  BYTE textcolor, textbgcolor;
  BYTE textsize, rotation;

  bool wrap;

  SSD1306 *display;
};

#endif // _ADAFRUIT_GFX_H
