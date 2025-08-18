#ifndef __DMA_HANDLERS_H
#define __DMA_HANDLERS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "circularbuffer.h"

void updateWriteDataDirect(uint16_t capturedStamp);
void updateReadTimerDma(uint8_t lowerNotUpper);
void processWriteTimerDma(uint8_t lowerNotUpper);
void updateWriteDataDirect(uint16_t capturedStamp);
void DMA1_Channel1_IRQHandler(void);
void DMA1_Channel2_3_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* __DMA_HANDLERS_H */
