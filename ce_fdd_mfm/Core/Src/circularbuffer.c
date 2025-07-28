#include "circularbuffer.h"

uint32_t txCnt;
uint8_t txData[TX_DATA_SIZE];

volatile uint32_t rxCnt;
uint32_t rxLoad;
uint8_t rxData[BFR_SIZE];
