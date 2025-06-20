#include "circularbuffer.h"

uint8_t txCnt, txStore, txLoad;
uint8_t txData[BFR_SIZE];

uint8_t rxCnt, rxStore, rxLoad;
uint8_t rxData[BFR_SIZE];
