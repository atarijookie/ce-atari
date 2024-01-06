#ifndef _INIT_H_
#define _INIT_H_

void dma_mfmRead_init(void);
void spi_init(void);
void spi_fillInitStruct(void);
void dma_mfmWrite_init(void);
void dma_spi_init(void);
void init_hw_sw(void);
void initUsarts(void);
void Exti3InterruptOn(BYTE onNotOff);
void reconfigV1V2V4pins(BYTE mode);

#endif
