#include "stm32f10x.h"                       // STM32F103 definitions
#include "stm32f10x_spi.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_dma.h"
#include "stm32f10x_exti.h"
#include "misc.h"

#include "datatransfer.h"

#include "bridge.h"
extern BYTE brStat;                     // status from bridge
extern BYTE isAcsiNotScsi;

BYTE dataReadCloop_acsi(WORD *pData, WORD dataCnt);
BYTE dataReadCloop_scsi(WORD *pData, WORD dataCnt);

__forceinline BYTE timeout(void)
{
	if((TIM3->SR & 0x0001) != 0) {		// overflow of TIM4 occured?
		TIM3->SR = 0xfffe;							// clear UIF flag
		return TRUE;
	}
	
	return FALSE;
}

//--------------------------------
// for ACSI

#define READ_WORD	\
		data = *pData;\
		hi = data >> 8;\
		GPIOB->ODR = hi;\
		GPIOA->BSRR	= aDMA;\
		__asm  { nop } \
		GPIOA->BRR	= aDMA;\
		lo = data & 0xff;\
		while(1) {\
			if(timeout()) {\
                brStat = E_TimeOut; \
				return 0;\
			}\
			exti = EXTI->PR;\
			if(exti & aACK) {\
				EXTI->PR = aACK;\
				break;\
			}\
		}\
		GPIOB->ODR = lo;\
		GPIOA->BSRR	= aDMA;\
		__asm  { nop } \
		GPIOA->BRR	= aDMA;\
		pData++;\
		while(1) {\
			if(timeout()) {\
                brStat = E_TimeOut; \
				return 0;\
			}\
			exti = EXTI->PR;\
			if(exti & aACK) {\
				EXTI->PR = aACK;\
				break;\
			}\
		}

#define READ_BYTE \
		data = *pData;\
		pData++;\
		hi = data >> 8;\
		GPIOB->ODR = hi;\
		GPIOA->BSRR	= aDMA;\
		__asm  { nop } \
		GPIOA->BRR	= aDMA;\
		while(1) {\
			if(timeout()) {\
                brStat = E_TimeOut; \
				return 0;\
			}\
			exti = EXTI->PR;\
			if(exti & aACK) {\
				EXTI->PR = aACK;\
				break;\
			}\
		}

//--------------------------------
// for SCSI

#define READ_WORD_SCSI \
		data = *pData;\
		hi = data >> 8;\
		GPIOB->ODR = hi;\
        while(1) { \
            WORD wVal = GPIOB->IDR; \
            if((wVal & aACK) != 0) { \
                break; \
            } \
            if(timeout()) { \
                brStat = E_TimeOut; \
				return 0;\
            } \
        } \
		GPIOA->BSRR	= aDMA;\
		__asm  { nop } \
		GPIOA->BRR	= aDMA;\
		lo = data & 0xff;\
		while(1) {\
			if(timeout()) {\
                brStat = E_TimeOut; \
				return 0;\
			}\
			exti = EXTI->PR;\
			if(exti & aACK) {\
				EXTI->PR = aACK;\
				break;\
			}\
		}\
		GPIOB->ODR = lo;\
        while(1) { \
            WORD wVal = GPIOB->IDR; \
            if((wVal & aACK) != 0) { \
                break; \
            } \
            if(timeout()) { \
                brStat = E_TimeOut; \
				return 0;\
            } \
        } \
		GPIOA->BSRR	= aDMA;\
		__asm  { nop } \
		GPIOA->BRR	= aDMA;\
		pData++;\
		while(1) {\
			if(timeout()) {\
                brStat = E_TimeOut; \
				return 0;\
			}\
			exti = EXTI->PR;\
			if(exti & aACK) {\
				EXTI->PR = aACK;\
				break;\
			}\
		}

#define READ_BYTE_SCSI \
		data = *pData;\
		pData++;\
		hi = data >> 8;\
		GPIOB->ODR = hi;\
        while(1) { \
            WORD wVal = GPIOB->IDR; \
            if((wVal & aACK) != 0) { \
                break; \
            } \
            if(timeout()) { \
                brStat = E_TimeOut; \
				return 0;\
            } \
        } \
		GPIOA->BSRR	= aDMA;\
		__asm  { nop } \
		GPIOA->BRR	= aDMA;\
		while(1) {\
			if(timeout()) {\
                brStat = E_TimeOut; \
				return 0;\
			}\
			exti = EXTI->PR;\
			if(exti & aACK) {\
				EXTI->PR = aACK;\
				break;\
			}\
		}

//--------------------------------        
        
BYTE dataReadCloop(WORD *pData, WORD dataCnt)
{
    if(isAcsiNotScsi) {
        return dataReadCloop_acsi(pData, dataCnt);
    } else {
        return dataReadCloop_scsi(pData, dataCnt);
    }
}            

BYTE dataReadCloop_acsi(WORD *pData, WORD dataCnt)
{
	WORD exti;

	while(dataCnt > 0) {
		WORD data;
		BYTE hi,lo;
		
		// if got at least 16 bytes, read 16 bytes
		if(dataCnt >= 16) {
			READ_WORD
			READ_WORD
			READ_WORD
			READ_WORD
			READ_WORD
			READ_WORD
			READ_WORD
			READ_WORD
			dataCnt = dataCnt - 16;
			continue;
		}

		// if got at least 2 bytes, read 2 bytes
		if(dataCnt >= 2) {
			READ_WORD
			dataCnt = dataCnt - 2;
			continue;
		}
		
		// if got 1 byte
		READ_BYTE
		dataCnt = dataCnt - 1;
	}
	
	return 1;
}

BYTE dataReadCloop_scsi(WORD *pData, WORD dataCnt)
{
	WORD exti;

	while(dataCnt > 0) {
		WORD data;
		BYTE hi,lo;
		
		// if got at least 16 bytes, read 16 bytes
		if(dataCnt >= 16) {
			READ_WORD_SCSI
			READ_WORD_SCSI
			READ_WORD_SCSI
			READ_WORD_SCSI
			READ_WORD_SCSI
			READ_WORD_SCSI
			READ_WORD_SCSI
			READ_WORD_SCSI
			dataCnt = dataCnt - 16;
			continue;
		}

		// if got at least 2 bytes, read 2 bytes
		if(dataCnt >= 2) {
			READ_WORD_SCSI
			dataCnt = dataCnt - 2;
			continue;
		}
		
		// if got 1 byte
		READ_BYTE_SCSI
		dataCnt = dataCnt - 1;
	}
	
	return 1;
}

