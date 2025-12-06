#ifndef __DISPLAY_H__
#define __DISPLAY_H__

#define DISPLAY_I2C_ADDRESS     0x3C
#define DISPLAY_I2C_IFACE       i2c1

void displayInit(void);
void displayMessage(const char* msg1 = NULL, const char* msg2 = NULL, const char* msg3 = NULL);

#endif
