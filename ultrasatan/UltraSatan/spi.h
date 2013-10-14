#ifndef SPI_H
#define SPI_H

#define SPI_FREQ_SLOW	0
#define SPI_FREQ_12MHZ	1
#define SPI_FREQ_16MHZ	2
#define SPI_FREQ_22MHZ	3

void spiInit(void);
void spiSendByte(BYTE data);
BYTE spiTransferByte(BYTE data);
WORD spiTransferWord(WORD data);

//void spiChipSelect(BYTE cs);
void spiCShigh(BYTE cs);
void spiCSlow(BYTE cs);

void spiSetSPCKfreq(BYTE which);

void spiInterrupt(void);

BYTE spiReceiveByte(void);

extern void spiSendByteAsm(BYTE data);
extern BYTE spiTransferByteAsm(BYTE data);
extern void MMC_ReadAsm(void);
extern BYTE spiReceiveByteAsm(WORD i);
extern WORD SPIreadSectorAsm(void);

extern WORD SPIwriteSectorAsm(void);
#endif

