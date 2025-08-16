#ifndef CIRCULARBUFFER_H_
#define CIRCULARBUFFER_H_

#include <stdint.h>

/*
 * With 512 bytes buffer we can stream almost 8 ms without refilling,
 * this should give host enough time to handle other stuff.
 *
 * Transfer in 256 bytes blocks.
 * RXE pin will indicate if at least 256 bytes can be RXed.
 */

#define BFR_SIZE            512
#define BFR_SIZE_HALF       (BFR_SIZE / 2)
#define BFR_MASK            0x1FF
#define BFR_SIZE_CAN_RX     BFR_SIZE_HALF

/*
 * Maximum bytes with mfm symbols count in written sector == write buffer size
 * - you get the most symbols in sector, if all data bytes are zeros or all ones
 * - if you write header + data: 1189 bytes (you write this only when formatting?)
 * - if you write just data part: 1096 bytes (you write this when you write new sector data)
 * This means that 1200 bytes should be enough for written data.
 */
#define WRITEBUFFER_SIZE    1200

extern uint32_t txCnt;
extern uint8_t txData1[WRITEBUFFER_SIZE], txData2[WRITEBUFFER_SIZE];

extern volatile uint32_t rxCnt;
extern uint32_t rxLoad;
extern uint8_t rxData[BFR_SIZE];

#define RX_CLEAR()    { rxCnt = 0; rxLoad = 0;                                                           }
#define RX_GET()     ({ rxCnt--; uint8_t retVal = rxData[rxLoad];         rxLoad  = (rxLoad  + 1) & BFR_MASK; retVal; })
#define RX_DROP()     { rxCnt--;                                          rxLoad  = (rxLoad  + 1) & BFR_MASK;         }

#define UPDATE_PIN_RXE  { GPIOA->BSRR = (rxCnt <= BFR_SIZE_CAN_RX) ? PIN_RXE : (PIN_RXE << 16); }    // H if read buffer getting low

#endif
