#ifndef __DISPLAY_H__
#define __DISPLAY_H__

#include "hardware/i2c.h"

#define DISPLAY_I2C_ADDRESS     0x3C
#define DISPLAY_I2C_IFACE       i2c1

bool isI2CdeviceConnected(uint8_t address);
void displayInit(void);
void displayMessage(const char* msg1 = NULL, const char* msg2 = NULL, const char* msg3 = NULL);

#endif
