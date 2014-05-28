#include "stm32f10x_spi.h"

#include "mmc.h"
#include "defs.h"
#include "bridge.h"

extern TDevice sdCard;
extern unsigned char brStat;

#define SPIBUFSIZE  (3 * 512)
BYTE spiTxBuff[SPIBUFSIZE];
BYTE spiRxBuff1[SPIBUFSIZE];
BYTE spiRxBuff2[SPIBUFSIZE];

void waitForSpi2Finish(void);
void stopSpi2Dma(void);
void waitForSPI2idle(void);

void spi2Dma_txRx(WORD txCount, BYTE *txBfr, WORD rxCount, BYTE *rxBfr);

#define READ_BYTE_BFR \
		byte = *pData;\
		pData++;\
		GPIOB->ODR = byte;\
		GPIOA->BSRR	= aDMA;\
		__asm  { nop } \
		GPIOA->BRR	= aDMA;\
		while(1) {\
			if(timeout()) {\
                quit = 1;\
                break;\
			}\
			exti = EXTI->PR;\
			if(exti & aACK) {\
				EXTI->PR = aACK;\
				break;\
			}\
		}\
        if(quit) {\
            break;\
        }
        
BYTE mmcRead_dma(DWORD sector, WORD count)
{
    BYTE r1, quit;
    WORD i;
    BYTE byte = 0;
    
    BYTE    *pData;
    WORD    gotDataCnt;
    DWORD   dataReadCount;
    WORD    thisDataCount, lastDataCount = 0;
    WORD    dataCnt;
    BYTE    *rxBuffNow;
    BYTE    transferedWholeSector = TRUE;
    WORD    exti;
    
    timeoutStart();
    
    // assert chip select
    spiCSlow();                                             // CS to L

    if(sdCard.Type != DEVICETYPE_SDHC)                      // for non SDHC cards change sector into address
        sector = sector<<9;

    // issue command
    r1 = mmcCommand(MMC_READ_MULTIPLE_BLOCKS, sector);

    // check for valid response
    if(r1 != 0x00) {
        spi2_TxRx(0xFF);
        spiCShigh();                                        // CS to H
        return r1;
    }

    // read in data
    ACSI_DATADIR_READ();
    
    quit = 0;

    for(i=0; i<SPIBUFSIZE; i++) {                               // fill the TX buffer with FFs
        spiTxBuff[i] = 0xff;
    }
    
    dataReadCount = ((DWORD) count) << 9;                       // change sector count to byte count
    
    gotDataCnt  = 0;
    pData       = spiRxBuff1;
    
    rxBuffNow = spiRxBuff1;
    spi2Dma_txRx(SPIBUFSIZE, spiTxBuff, SPIBUFSIZE, rxBuffNow); // start first transfer

    while(dataReadCount > 0)                                    // read this many bytes
    {
        // if we don't have data, we should get some using DMA
        if(gotDataCnt == 0) {                                   // if don't have data
            waitForSpi2Finish();                                // wait until we have data
            
            pData       = rxBuffNow;                            // point at start of RX buffer
            gotDataCnt  = SPIBUFSIZE;
            
            if(rxBuffNow == spiRxBuff1) {                       // we were transfering to buf 1? 
                rxBuffNow = spiRxBuff2;                         // now transfer to buf 2
            } else {
                rxBuffNow = spiRxBuff1;                         // if we were transfering to buf 2, transfer to buf 1
            }
                
            spi2Dma_txRx(SPIBUFSIZE, spiTxBuff, SPIBUFSIZE, rxBuffNow);  // start next transfer
        }
        
        // depending on the previous whole / partial sector transfer, do or don't look for data marker
        if(transferedWholeSector) {                             // if in the previous loop we've sent the whole sector, look for new data marker
            byte = 0;
        
            while(gotDataCnt > 0) {                             // search RX buffer for start of data
                byte = *pData;                                  // get data
                pData++;                                        // move pointer forward
                gotDataCnt--;
                
                if(byte == MMC_STARTBLOCK_READ) {               // block start found?
                    break;
                }
            }
        
            if(byte != MMC_STARTBLOCK_READ) {                   // didn't find the start of block? request a new one
                gotDataCnt = 0;
                continue;
            }

            thisDataCount = (gotDataCnt >= 512) ? 512 : gotDataCnt; // how much data to transfer
        } else {
            thisDataCount = 512 - lastDataCount;                // transfer only the rest of sector
        }
        
        // transfer the bytes in the loop to ST
        dataCnt = thisDataCount;
        
        while(dataCnt > 0) {
            READ_BYTE_BFR
            dataCnt--;
        }
 
        if(quit) {                                              // if error happened
            break;
        }
        
        // after one whole / partial sector, update the variables and try again
        dataReadCount   -= thisDataCount;                       // update how many data we still need to transfer
        gotDataCnt      -= thisDataCount;                       // update how many data we have buffered

        if(transferedWholeSector == FALSE) {                    // if we transfered the rest of previous sector, continue as if transfered whole sector
            thisDataCount = 512;
        }
        
        // depending on if transfered whole sector or not, then the next transfer will be whole sector or not...
        if(thisDataCount == 512) {                              // if transfered whole sector
            pData       += 2;                                   // skip CRC
            
            if(gotDataCnt >= 2) {
                gotDataCnt -= 2;                                // remove 2 CRC bytes from gotDataCnt
            } else {
                gotDataCnt = 0;
            }
            
            transferedWholeSector = TRUE;
        } else {                                                // if we didn't transfer whole sector
            transferedWholeSector = FALSE;
            lastDataCount = thisDataCount;                      // this will be used how much we need to transfer to transfer the rest
        }
    }
    //-------------------------------
    // stop SPI2 DMA if it is running
    stopSpi2Dma();
    
    // stop the transmition of next sector
    mmcCommand(MMC_STOP_TRANSMISSION, 0);                   // send command instead of CRC
    spi2_TxRx(0xFF);
    
    // release chip select
    spiCShigh();         // CS to H

    if(quit) {                                              // if error happened
        return 0xff;                                        // error
    }
    
    return 0;                                               // OK   
}

void waitForSpi2Finish(void)
{
    while(1) {
        if(DMA1_Channel5->CNDTR == 0 && DMA1_Channel4->CNDTR == 0) {        // both channels have nothing to do? quit
            break;
        }
    }
}

void stopSpi2Dma(void)
{
    DMA1_Channel5->CCR &= 0xfffffffe;                  // disable DMA5 Channel transfer
    DMA1_Channel4->CCR &= 0xfffffffe;                  // disable DMA4 Channel transfer
    
    DMA1_Channel5->CNDTR = 0;
    DMA1_Channel4->CNDTR = 0;
    
    waitForSPI2idle();
}
