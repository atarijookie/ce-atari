#ifndef CIRCULARBUFFER_H_
#define CIRCULARBUFFER_H_

#include <stdint.h>

#define BFR_SIZE        128
#define BFR_MASK        0x7F
#define BFR_SIZE_HALF   (BFR_SIZE/2)


extern uint8_t txCnt, txStore, txLoad;
extern uint8_t txData[BFR_SIZE];

extern uint8_t rxCnt, rxStore, rxLoad;
extern uint8_t rxData[BFR_SIZE];

#define TX_CLEAR()    { txCnt = 0; txStore = 0; txLoad = 0;                                                           }
#define TX_ADD(VAL)   { txCnt++;                  txData[txStore] = VAL;  txStore = (txStore + 1) & BFR_MASK;         }
#define TX_GET()     ({ txCnt--; uint8_t retVal = txData[txLoad];         txLoad  = (txLoad  + 1) & BFR_MASK; retVal; })

#define RX_CLEAR()    { rxCnt = 0; rxStore = 0; rxLoad = 0;                                                           }
#define RX_ADD(VAL)   { rxCnt++;                  rxData[rxStore] = VAL;  rxStore = (rxStore + 1) & BFR_MASK;         }
#define RX_GET()     ({ rxCnt--; uint8_t retVal = rxData[rxLoad];         rxLoad  = (rxLoad  + 1) & BFR_MASK; retVal; })

#endif
