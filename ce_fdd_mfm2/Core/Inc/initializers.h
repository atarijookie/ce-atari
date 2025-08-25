#ifndef __INITIALIZERS_H
#define __INITIALIZERS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g030xx.h"

void setupSpiUsingCircularDma(void);
void setupTMIcircularDma(DMA_Channel_TypeDef* dmaChannel, TIM_TypeDef* timer, uint16_t* pBfr, int bfrSize, int isOutput);

void dmaReconfigForRead(void);
void dmaReconfigForWrite(void);

void spiDmaTxBuffer(uint32_t pData, uint32_t count);

#ifdef __cplusplus
}
#endif

#endif /* __INITIALIZERS_H */
