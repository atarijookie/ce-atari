#include "circularbuffer.h"

volatile uint32_t rxCnt;
uint32_t rxLoad;
uint8_t rxData[BFR_SIZE];
