#ifndef __INITIALIZERS_H
#define __INITIALIZERS_H

#ifdef __cplusplus
extern "C" {
#endif

void setupSpiUsingCircularDma(void);
void setupTMI3circularDma(void);

void dmaReconfigForRead(void);
void dmaReconfigForWrite(void);

void spiDmaTxBuffer(uint32_t pData, uint32_t count);
void spiDmaTxZeros(void);

#ifdef __cplusplus
}
#endif

#endif /* __INITIALIZERS_H */
