#include "stm32f10x_spi.h"

#include "mmc.h"
#include "defs.h"
#include "bridge.h"

extern TDevice sdCard;
extern unsigned char brStat;
extern BYTE isAcsiNotScsi;

extern BYTE mode;
extern volatile BYTE spiDmaIsIdle;
void sendFwToHost(void);

#define SPIBUFSIZE  520
BYTE spiTxBuff[SPIBUFSIZE];
BYTE spiRxBuff1[SPIBUFSIZE];
BYTE spiRxBuff2[SPIBUFSIZE];
BYTE spiRxDummy[SPIBUFSIZE];

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
                LOG_ERROR(10);\
                brStat = E_TimeOut; \
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
        
// the following macro waits until the card is busy, and when it's not busy, it continues        
#define WAIT_FOR_CARD_NOT_BUSY              \
        while(spi2_TxRx(0xFF) != 0xff) {    \
            if(timeout()) {                 \
                LOG_ERROR(11);              \
                quit = 1;                   \
                break;                      \
            }                               \
        }

BYTE waitForCardNotBusy(void)
{
    while(spi2_TxRx(0xFF) != 0xff) {                    // while card still returns something non-zero

        if(mode != MODE_SOLO) {                         // do this only when we're NOT in solo mode
            if(spiDmaIsIdle && (TIM2->SR & 0x0001)) {   // SPI DMA is idle, and it's time to send FW report
                TIM2->SR = 0xfffe;                      // clear UIF flag

                sendFwToHost();
            }
        }

        if(timeout()) {                                 // if timeout happened, quit
            LOG_ERROR(11);
            return 1;
        }
    }

    return 0;
}

BYTE mmcRead_dma(DWORD sector, WORD count)
{
    BYTE r1, quit, good;
    WORD i,c, last;
    BYTE byte = 0;
    
    BYTE    *pData;
    BYTE    *rxBuffNow;
    WORD    exti;
    WORD    waits, retries;

    if(sdCard.Type != DEVICETYPE_SDHC)                      // for non SDHC cards change sector into address
        sector = sector<<9;
    
    // read in data
    ACSI_DATADIR_READ();
    
    quit = 0;

    for(i=0; i<SPIBUFSIZE; i++) {                               // fill the TX buffer with FFs
        spiTxBuff[i] = 0xff;
    }
    
    pData     = spiRxBuff1;
    rxBuffNow = spiRxBuff1;

    timeoutStart();
    
    //---------------
    // try to start READ MULTIPLE BLOCKS with couple of retries
    for(retries=0; retries<5; retries++) {
        if(timeout()) {                                         // timeout through timer? quit with fail
            LOG_ERROR(12);
            return 0xff;
        }
        
        spiCSlow();                                             // CS to L

        // issue command
        r1 = mmcCommand(MMC_READ_MULTIPLE_BLOCKS, sector);

        // check for valid response
        if(r1 != 0x00) {
            spi2_TxRx(0xFF);
            spiCShigh();                                        // CS to H
            return r1;
        }
    
        good = 0;
        for(waits=0; waits < 0xffff; waits++) {                 // wait for STARTBLOCK
            r1 = spi2_TxRx(0xFF);
            
            if(r1 == MMC_STARTBLOCK_READ) {                     // if received STARTBLOCK, then we're good, quit this loop
                good = 1;
                break;
            }
        }
        
        if(good) {                                              // good? also quit this loop
            break;
        }
        
        spiCShigh();                                            // CS to H
        spi2_TxRx(0xFF);
        spi2_TxRx(0xFF);
    }

    if(!good) {                                                 // not good? READ FAILED :(
        return 0xff;
    }
    //---------------
    
    spi2Dma_txRx(512, spiTxBuff, 512, rxBuffNow);               // start first transfer

    last = count - 1;
    
    for(c=0; c<count; c++) {
        waitForSpi2Finish();                                    // wait until we have data
        waitForSPI2idle();                                      // now wait until SPI2 finishes

        pData = rxBuffNow;                                      // point at start of RX buffer
            
        if(rxBuffNow == spiRxBuff1) {                           // we were transfering to buf 1? 
            rxBuffNow = spiRxBuff2;                             // now transfer to buf 2
        } else {
            rxBuffNow = spiRxBuff1;                             // if we were transfering to buf 2, transfer to buf 1
        }

        if(c != last) {                                         // need to read more than this sector? start transfer of the next one
            spi2_TxRx(0xFF);                                    // read out CRC
            spi2_TxRx(0xFF);
            
            while(spi2_TxRx(0xFF) != MMC_STARTBLOCK_READ) {     // wait for STARTBLOCK
                if(timeout()) {
                    LOG_ERROR(13);
                    quit = 1;
                    break;
                }
            }
            
            spi2Dma_txRx(512, spiTxBuff, 512, rxBuffNow);       // start first transfer
        }
            
        // transfer the bytes in the loop to ST
        for(i=0; i<512; i++) {
            READ_BYTE_BFR
        }
        
        if(quit) {                                              // if error happened
            break;
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

BYTE mmcWrite_dma(DWORD sector, WORD count)
{
    BYTE r1, quit;
    WORD i,j;
    DWORD thisSector;
    BYTE *pSend;
    BYTE *pData;
//  BYTE r1a=0xff, r1b=0xff;

    timeoutStart();
    
    //-----------
/*
    // for SD and SDHC, send SET BLOCK COUNT command before WRITE MULTIPLE BLOCKS
    if(sdCard.Type == DEVICETYPE_SDHC || sdCard.Type == DEVICETYPE_SD) {
        r1a = mmcSendCommand(SD_APP_CMD, 0);        // ACMD23 = CMD55 + CMD23

        if(r1a < 2) {                               // good? send the rest
            r1b = mmcSendCommand(SD_SET_BLOCK_COUNT, count);
        }
    }
*/
    //-----------

    // assert chip select
    spiCSlow();          // CS to L

    thisSector = sector;
    
    if(sdCard.Type != DEVICETYPE_SDHC)          // for non SDHC cards change sector into address
        sector = sector<<9;

    //--------------------------
    // issue command
    r1 = mmcCommand(MMC_WRITE_MULTIPLE_BLOCKS, sector);

    // check for valid response
    if(r1 != 0x00) {
        spi2_TxRx(0xFF);
        spiCShigh();                            // CS to H
        LOG_ERROR(60);
        return r1;
    }
    
    //--------------
    // read in data
    ACSI_DATADIR_WRITE();
    
    quit = 0;
    
    pData = spiRxBuff1;                         // start storing to this buffer
    //--------------
    for(j=0; j<count; j++) {                    // read this many sectors
        //--------------
        // keep sending FW version to notify host that we're alive
        if(mode != MODE_SOLO) {                         // do this only when we're NOT in solo mode
            if(spiDmaIsIdle && (TIM2->SR & 0x0001)) {   // SPI DMA is idle, and it's time to send FW report
                TIM2->SR = 0xfffe;                      // clear UIF flag

                sendFwToHost();
            }
        }

        //--------------
        if(thisSector >= sdCard.SCapacity) {    // sector out of range?
            quit = 1;
            LOG_ERROR(61);
            break;                              // quit
        }
        //--------------    
        *pData = MMC_STARTBLOCK_MWRITE;         // 0xfc as start write multiple blocks
        pData++;

        for(i=0; i<512; i++)                    // read this many bytes
        {
            r1 = DMA_write();                   // get it from ST
            
            *pData = r1;
            pData++;
            
            if(brStat != E_OK) {                // if something was wrong
                quit = 1;
                LOG_ERROR(62);
                break;                          // quit
            }
        }           

        if(quit) {                              // if error happened
            break; 
        }
        
        *pData = 0xff;                          // store 16-bit CRC
        pData++;
        *pData = 0xff;                          // store 16-bit CRC
        pData++;

        if((j & 1) == 0) {                      // for even sectors
            pSend   = spiRxBuff1;
            pData   = spiRxBuff2;
        } else {
            pSend   = spiRxBuff2;
            pData   = spiRxBuff1;
        }

        waitForSpi2Finish();                        // wait until DMA finishes
        waitForSPI2idle();                          // now wait until SPI2 finishes
 
        // wait while busy
        quit = waitForCardNotBusy();
            
        if(quit) {
            break;
        }
        
        spi2Dma_txRx(515, pSend, 515, spiRxDummy);  // send this data in the backgroun
        
        thisSector++;                               // increment real sector #
    }
    //-------------------------------
    waitForSpi2Finish();                            // wait until DMA finishes
    waitForSPI2idle();                              // now wait until SPI2 finishes

    // wait while busy
    WAIT_FOR_CARD_NOT_BUSY

    spi2_TxRx(MMC_STOPTRAN_WRITE);                  // 0xfd to stop write multiple blocks

    // wait while busy
    WAIT_FOR_CARD_NOT_BUSY

    //-------------------------------
    // for the MMC cards send the STOP TRANSMISSION also
    if(sdCard.Type == DEVICETYPE_MMC) {      
        mmcCommand(MMC_STOP_TRANSMISSION, 0);

        // wait while busy
        WAIT_FOR_CARD_NOT_BUSY
    }
    //-------------------------------
    spi2_TxRx(0xFF);
    spiCShigh();                                // CS to H
    
    if(quit) {                                  // if failed, return error
        return 0xff;
    }
        
    return 0;                                   // success
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
