#ifndef _CART_H_
#define _CART_H_

#include "global.h"

// from https://www.atarimagazines.com/st-log/issue27/138_1_A_16-BIT_CARTRIDGE_PORT_INTERFACE.php
// "UDS for even bytes, and LDS for odd"
// Cart status is on UDS - even byte.
// Cart data   is on LDS - odd byte.

#define CART_BASE               0xFB0000

#define CART_STATUS             0xFB0000
#define CART_DATA               0xFB0001

#define pCartStatus ((volatile BYTE *) CART_STATUS)
#define pCartData   ((volatile BYTE *) CART_DATA)

// status bits when cart is read as WORD
#define STATUS_W_RPIisIdle      (1 << 10)   // 1 when RPi doesn't do any further transfer (last byte was status byte)
#define STATUS_W_RPIwantsMore   (1 <<  9)   // 1 when ST should transfer another byte (read or write)
#define STATUS_W_DataChanged    (1 <<  8)   // this bit changes every time the data changed (0->1, 1->0)

// status bits when cart is read as BYTE
#define STATUS_B_RPIisIdle      (1 <<  2)   // 1 when RPi doesn't do any further transfer (last byte was status byte)
#define STATUS_B_RPIwantsMore   (1 <<  1)   // 1 when ST should transfer another byte (read or write)
#define STATUS_B_DataChanged    (1 <<  0)   // this bit changes every time the data changed (0->1, 1->0)

#define TIMEOUT         200         // timeout 1 sec

void cart_cmd(BYTE ReadNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount);

#endif
