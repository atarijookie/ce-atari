#ifndef CIRCULARBUFFER_H_
#define CIRCULARBUFFER_H_

#include <stdint.h>
#include "defs.h"

#define RX_CLEAR()    { rxCnt = 0; rxLoad = 0;                                                           }
#define RX_GET()     ({ rxCnt--; uint8_t retVal = rxData[rxLoad];         rxLoad  = (rxLoad  + 1) & BFR_MASK; retVal; })
#define RX_DROP()     { rxCnt--;                                          rxLoad  = (rxLoad  + 1) & BFR_MASK;         }

#define UPDATE_PIN_RXE  { GPIOA->BSRR = (rxCnt <= BFR_SIZE_CAN_RX) ? PIN_RXE : (PIN_RXE << 16); }    // H if read buffer getting low

#endif
