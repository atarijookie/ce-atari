#ifndef  _INIT_H_
#define  _INIT_H_

void dma_mfmRead_init(void);
void spi_init(void);
void dma_mfmWrite_init(void);
void dma_spi_init(void);
void init_hw_sw(void);
void Exti3InterruptOn(BYTE onNotOff);

#endif
