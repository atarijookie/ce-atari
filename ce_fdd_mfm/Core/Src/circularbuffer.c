#include "circularbuffer.h"

//uint32_t txCnt, txStore, txLoad;
uint32_t txCnt;
uint8_t txData[TX_DATA_SIZE];

volatile uint32_t rxCnt;
//rxStore,
uint32_t rxLoad;
uint8_t rxData[BFR_SIZE];
