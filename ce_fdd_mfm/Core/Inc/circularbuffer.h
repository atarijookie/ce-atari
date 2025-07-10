#ifndef CIRCULARBUFFER_H_
#define CIRCULARBUFFER_H_

#include <stdint.h>

/*
 * With 512 bytes buffer we can stream almost 8 ms without refilling,
 * this should give host enough time to handle other stuff.
 *
 * Transfer in 32 bytes blocks.
 * RXE pin will indicate if at least 32 bytes can be RXed.
 */

#define BFR_SIZE            512
#define BFR_MASK            0x1FF
#define BFR_SIZE_CAN_RX     (BFR_SIZE - 64)


extern uint32_t txCnt, txStore, txLoad;
extern uint8_t txData[BFR_SIZE];

extern uint32_t rxCnt, rxStore, rxLoad;
extern uint8_t rxData[BFR_SIZE];

#define TX_CLEAR()    { txCnt = 0; txStore = 0; txLoad = 0;                                                           }
#define TX_PUT(VAL)   { txCnt++;                  txData[txStore] = VAL;  txStore = (txStore + 1) & BFR_MASK;         }
#define TX_GET()     ({ txCnt--; uint8_t retVal = txData[txLoad];         txLoad  = (txLoad  + 1) & BFR_MASK; retVal; })

#define RX_CLEAR()    { rxCnt = 0; rxStore = 0; rxLoad = 0;                                                           }
#define RX_PUT(VAL)   { rxCnt++;                  rxData[rxStore] = VAL;  rxStore = (rxStore + 1) & BFR_MASK;         }
#define RX_GET()     ({ rxCnt--; uint8_t retVal = rxData[rxLoad];         rxLoad  = (rxLoad  + 1) & BFR_MASK; retVal; })
#define RX_DROP()     { rxCnt--;                                          rxLoad  = (rxLoad  + 1) & BFR_MASK;         }

#define UPDATE_PIN_RXE  { GPIOA->BSRR = (rxCnt <= BFR_SIZE_CAN_RX) ? PIN_RXE : (PIN_RXE << 16); }    // H if read buffer getting low

#endif
