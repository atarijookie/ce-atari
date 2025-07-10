#include "circularbuffer.h"

uint32_t txCnt, txStore, txLoad;
uint8_t txData[BFR_SIZE];

uint32_t rxCnt, rxStore, rxLoad;
uint8_t rxData[BFR_SIZE];
