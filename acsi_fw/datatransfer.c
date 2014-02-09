#include "stm32f10x.h"                       // STM32F103 definitions
#include "stm32f10x_spi.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_dma.h"
#include "stm32f10x_exti.h"
#include "misc.h"

#include "datatransfer.h"

#define READ_WORD	\
		data = *pData;\
		hi = data >> 8;\
		GPIOB->ODR = hi;\
		GPIOA->BSRR	= aDMA;\
		GPIOA->BRR	= aDMA;\
		lo = data & 0xff;\
		while(1) {\
			if(timeCnt == 0) {\
				return 0;\
			}\
			timeCnt--;\
			exti = EXTI->PR;\
			if(exti & aACK) {\
				EXTI->PR = aACK;\
				break;\
			}\
		}\
		GPIOB->ODR = lo;\
		GPIOA->BSRR	= aDMA;\
		GPIOA->BRR	= aDMA;\
		pData++;\
		while(1) {\
			if(timeCnt == 0) {\
				return 0;\
			}\
			timeCnt--;\
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
		GPIOA->BRR	= aDMA;\
		while(1) {\
			if(timeCnt == 0) {\
				return 0;\
			}\
			timeCnt--;\
			exti = EXTI->PR;\
			if(exti & aACK) {\
				EXTI->PR = aACK;\
				break;\
			}\
		}
		
#define READ_DWORD	\
		ddata = *pdData;\
		byte = ddata >> 8;\
		GPIOB->ODR = byte;\
		GPIOA->BSRR	= aDMA;\
		GPIOA->BRR	= aDMA;\
		byte = ddata & 0xff;\
		while(1) {\
			if(timeCnt == 0) {\
				return 0;\
			}\
			timeCnt--;\
			exti = EXTI->PR;\
			if(exti & aACK) {\
				EXTI->PR = aACK;\
				break;\
			}\
		}\
		GPIOB->ODR = byte;\
		GPIOA->BSRR	= aDMA;\
		GPIOA->BRR	= aDMA;\
		byte = ddata >> 24;\
		while(1) {\
			if(timeCnt == 0) {\
				return 0;\
			}\
			timeCnt--;\
			exti = EXTI->PR;\
			if(exti & aACK) {\
				EXTI->PR = aACK;\
				break;\
			}\
		}	\
		GPIOB->ODR = byte;\
		GPIOA->BSRR	= aDMA;\
		GPIOA->BRR	= aDMA;\
		byte = ddata >> 16;\
		while(1) {\
			if(timeCnt == 0) {\
				return 0;\
			}\
			timeCnt--;\
			exti = EXTI->PR;\
			if(exti & aACK) {\
				EXTI->PR = aACK;\
				break;\
			}\
		}\
		GPIOB->ODR = byte;\
		GPIOA->BSRR	= aDMA;\
		GPIOA->BRR	= aDMA;\
		pdData++;\
		while(1) {\
			if(timeCnt == 0) {\
				return 0;\
			}\
			timeCnt--;\
			exti = EXTI->PR;\
			if(exti & aACK) {\
				EXTI->PR = aACK;\
				break;\
			}\
		}				

BYTE dataReadCloop(WORD *pData, WORD dataCnt)
{
	DWORD timeCnt;
	WORD exti;
	DWORD *pdData = (DWORD *) pData;
	
	timeCnt = 0xfffff;
		
	while(dataCnt > 0) {
		DWORD ddata;
		WORD data;
		BYTE hi,lo,byte;
		
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

