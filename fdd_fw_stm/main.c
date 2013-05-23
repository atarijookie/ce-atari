#include "stm32f10x.h"                       /* STM32F103 definitions         */
#include "stm32f10x_spi.h"
#include "stm32f10x_tim.h"

#include "misc.h"

#include "defs.h"
#include "timers.h"

void getNextMfmTime(void);
void addAttention(BYTE atn);
void processHostCommand(BYTE hostByte);

void spi_init(void);
void spi_TxRx(void);

/*
TODO:
 - rozchodit IDR cez button pre STEP
 - rozchodit generovanie MFM streamu - pwm?
 - zosekanie / prepisanie do asm kvoli stihaniu obsluhy SPI + MFM
 - doriesit nacitanie dalsieho byte ked sa robi processHostCommand()
 - write support??
*/


// worst case scenario: 1 sector with all bullshit encoded in MFM should be max. 1228 bytes
// So a 4096 bytes big buffer should contain at least 3.3 sectors (one currently streamed, one received from host + something more)

// gap byte 0x4e == 666446 us = 222112 times = 10'10'10'01'01'10 = 0xa96
// 2 gap bytes 0x4e 0x4e = 0xa96a96 times = 0xa9 0x6a 0x96 bytes

// first gap (60 * 0x4e) takes 1920 us (1.9 ms)

BYTE inBuffer[4096];
WORD inIndexAdd, inIndexGet, inCount;

BYTE outBuffer[2048];
WORD outIndexAdd, outIndexGet, outCount;

BYTE mfmByte, mfmByteIndex;

BYTE side, track, sector;

// commands sent from device to host
#define CMD_SEND_NEXT_SECTOR    0x01               	// sent: 1, side (highest bit) + track #, current sector #
#define CMD_SECTOR_WRITTEN      0x02               	// sent: 2, side (highest bit) + track #, current sector #

// commands sent from host to device
#define CMD_WRITE_PROTECT_OFF		0x10
#define CMD_WRITE_PROTECT_ON		0x20
#define CMD_DISK_CHANGE_OFF			0x30
#define CMD_DISK_CHANGE_ON			0x40
#define CMD_CURRENT_SECTOR			0x50								// followed by sector #

#define MFM_4US     1
#define MFM_6US     2
#define MFM_8US     3

// set GPIOB as --- CNF1:0 -- 00 (push-pull output), MODE1:0 -- 11 (output 50 Mhz)
#define 	FloppyOut_Enable()			{GPIOB->CRH &= ~(0x00000fff); GPIOB->CRH |=  (0x00000333); FloppyIndex_Enable();	FloppyMFMread_Enable();		}
// set GPIOB as --- CNF1:0 -- 01 (floating input), MODE1:0 -- 00 (input)
#define 	FloppyOut_Disable()			{GPIOB->CRH &= ~(0x00000fff); GPIOB->CRH |=  (0x00000444); FloppyIndex_Disable();	FloppyMFMread_Disable();	}

// enable / disable TIM1 CH1 output on GPIOA_8
#define		FloppyIndex_Enable()		{ GPIOA->CRH &= ~(0x0000000f); GPIOA->CRH |= (0x0000000b); };
#define		FloppyIndex_Disable()		{ GPIOA->CRH &= ~(0x0000000f); GPIOA->CRH |= (0x00000004); };

// enable / disable TIM2 CH4 output on GPIOB_11
#define		FloppyMFMread_Enable()		{ GPIOB->CRH &= ~(0x0000f000); GPIOB->CRH |= (0x0000b000); };
#define		FloppyMFMread_Disable()		{ GPIOB->CRH &= ~(0x0000f000); GPIOB->CRH |= (0x00004000); };

// cyclic buffer add macros
#define 	outBuffer_add(X)				{ outBuffer[outIndexAdd]	= X;			outIndexAdd++;			outIndexAdd		= outIndexAdd & 0x7ff; 	outCount++; }
#define 	inBuffer_add(X)					{ inBuffer [inIndexAdd]		= X;			inIndexAdd++;				inIndexAdd		= inIndexAdd  & 0xfff;	inCount++;	}

// cyclic buffer get macros
#define		outBuffer_get(X)				{ X = outBuffer[outIndexGet];				outIndexGet++;			outIndexGet		= outIndexGet & 0x7ff;	outCount--;	}
#define		inBuffer_get(X)					{ X = inBuffer[inIndexGet];					inIndexGet++;				inIndexGet		= inIndexGet	& 0xfff;	inCount--;	}

int main (void) 
{
	BYTE prevSide, outputsActive;
	
  RCC->APB2ENR |= (1 << 12) | (1 << 11) | (1 << 3) | (1 << 2);     			// Enable SPI1, TIM1, GPIOA and GPIOB clock

	// set FLOATING INPUTs for GPIOB_0 ... 6
	GPIOB->CRL &= ~(0x0fffffff);						// remove bits from GPIOB
	GPIOB->CRL |=   0x04444444;							// set GPIOB as --- CNF1:0 -- 01 (floating input), MODE1:0 -- 00 (input), PxODR -- don't care

	FloppyOut_Disable();
	
	// enable atlernate function for PA4, PA5, PA6, PA7 == SPI
	GPIOA->CRL &= ~(0xffff0000);						// remove bits from GPIOA
	GPIOA->CRL |=   0xbbbb0000;							// set GPIOA as --- CNF1:0 -- 10 (push-pull), MODE1:0 -- 11, PxODR -- don't care

	// set GPIOB_15 (ATTENTION) as --- CNF1:0 -- 00 (push-pull output), MODE1:0 -- 11 (output 50 Mhz)
	GPIOB->CRH &= ~(0xf0000000); 
	GPIOB->CRH |=  (0x30000000);


	RCC->APB2ENR |= (1 << 0);									// enable AFIO
	AFIO->EXTICR[0] = 0x1000;									// EXTI3 -- source input: GPIOB_3
	EXTI->IMR			= STEP;											// EXTI3 -- 1 means: Interrupt from line 3 not masked
	EXTI->EMR			= STEP;											// EXTI3 -- 1 means: Event     form line 3 not masked
	EXTI->FTSR 		= STEP;											// Falling trigger selection register - STEP pulse
	
	//----------
	timerSetup_index();
	timerSetup_mfm();
	
	spi_init();																		// init SPI interface

// test of EXTI for STEP handling
while(1) {
	WORD ints = EXTI->PR;										// Pending register (EXTI_PR)
			
	if(ints & STEP) {										// if falling edge of STEP signal was found
		EXTI->PR = STEP;									// clear that int
	}
}

// test of MFM read stream and changes:
// ARPE = 0 -- no preloading, immediate change
// skontroluj ci TIMx->CR1 ma ARPE na 0!!!
while(1) {
	if((TIM2->SR & 0x0001) != 0) {		// overflow of TIM2 occured?
			TIM2->SR = 0xfffe;						// clear UIF flag
		
		// use period-1
		// TIMx->ARR = 7;								// 4 us
		// TIMx->ARR = 11;							// 6 us
		// TIMx->ARR = 15;							// 8 us
	}

}

/*
FloppyOut_Enable();
while(1) {
		WORD val = TIM1->SR;
		
		if((val & 0x0001) == 0) {			// no overflow? continue
			continue;
		}
		
		TIM1->SR = 0xfffe;			// clear UIF flag
		
		if(GPIOA->ODR & (1 << 10)) {
			GPIOA->BRR	= (1 << 10);
		} else {
			GPIOA->BSRR	= (1 << 10);
		}
	}
*/

	// init circular buffer for data incomming via SPI
	inIndexAdd		= 0;
	inIndexGet		= 0;
	inCount				= 0;
	
	// init circular buffer for data outgoing via SPI
	outIndexAdd		= 0;
	outIndexGet		= 0;
	outCount			= 0;

	mfmByte				= 0;
	mfmByteIndex	= 0;

	// init track and side vars for the floppy position
	side					= 0;
	track 				= 0;
	sector				= 1;
	prevSide			= 0;
	outputsActive	= 0;

	// init floppy signals
	GPIOB->BSRR = (WR_PROTECT | DISK_CHANGE);			// not write protected, not changing disk (ready!)
	GPIOB->BRR = TRACK0;													// TRACK 0 signal to L			

	while(1) {
		WORD ints;
		WORD inputs = GPIOB->IDR;								// read floppy inputs

		spi_TxRx();															// every 1 us might a SPI WORD be received! (16 MHz SPI clock)

		if((inputs & (MOTOR_ENABLE | DRIVE_SELECT)) != 0) {		// motor not enabled, drive not selected? do nothing
			if(outputsActive == 1) {							// if we got outputs active
				FloppyOut_Disable();								// all pins as inputs, drive not selected
				outputsActive = 0;
				
				// now that the floppy is disabled, drop the data from inBuffer, but process the host commands
				while(inCount > 0) {								// something in the buffer?
					BYTE hostByte;
					inBuffer_get(hostByte);						// get byte from host buffer

					if((hostByte & 0x0f) == 0) {			// lower nibble == 0? it's a command from host, process it; otherwise drop it
						processHostCommand(hostByte);
					}
				}
			}

			continue;
		}

		if(outputsActive == 0) {								// if the outputs are inactive
			FloppyOut_Enable();										// set these as outputs
			outputsActive = 1;
		}
		
		if((TIM2->SR & 0x0001) != 0) {			// overflow of TIM2 occured? we've streamed out one MFM symbol and need another!
			TIM2->SR = 0xfffe;								// clear UIF flag
			getNextMfmTime();									// get the next time from stream, set up TMR16B1
		}

		// check for STEP pulse - should we go to a different track?
		ints = EXTI->PR;										// Pending register (EXTI_PR)
		if(ints & STEP) {										// if falling edge of STEP signal was found
			EXTI->PR = STEP;									// clear that int

			if(inputs & DIR) {								// direction is High? track--
				if(track > 0) {
					track--;

					addAttention(CMD_SEND_NEXT_SECTOR);
				}
			} else {											// direction is Low? track++
				if(track < 82) {
					track++;

					addAttention(CMD_SEND_NEXT_SECTOR);
				}
			}

			if(track == 0) {									// if track is 0
				GPIOB->BRR = TRACK0;						// TRACK 0 signal to L			
			} else {													// if track is not 0
				GPIOB->BSRR = TRACK0;						// TRACK 0 signal is H
			}
		}

		//------------
		// update SIDE var
		side = (inputs & SIDE1) ? 0 : 1;					// get the current SIDE
		if(prevSide != side) {										// side changed?
			addAttention(CMD_SEND_NEXT_SECTOR);			// we need another sector, this time from the right side!
			prevSide = side;
		}

		//------------
		// check INDEX pulse as needed
		if((TIM1->SR & 0x0001) != 0) {		// overflow of TIM1 occured?
			int i;
			TIM1->SR = 0xfffe;							// clear UIF flag
			
			// create GAP1: 60 x 0x4e
			for(i=0; i<30; i++) {				// add 30* two encoded 4e marks into 3 bytes (2* 0xA96)
				inBuffer_add(0xa9);
				inBuffer_add(0x6a);
				inBuffer_add(0x96);
			}

			sector = 1;
			
			addAttention(CMD_SEND_NEXT_SECTOR);
		}
	}
}

void getNextMfmTime(void)
{
	// 0.4 us = 28 cycles (pulse low width)
	// 4 us = 288 cycles  (260 cycles)
	// 6 us = 432 cycles  (404 cycles)
	// 8 us = 576 cycles  (548 cycles)

	WORD mr0, mr1;
	
	static BYTE gap[3] = {0xa9, 0x6a, 0x96};
	static BYTE gapCnt = 0;

	BYTE time = mfmByte >> 6;								// get only 2 highest bits

	mfmByte = mfmByte << 2;									// move the mfmByte to next position
	mfmByteIndex++;

	switch(time) {											// convert time code to timer match values
	case MFM_8US:											// 8 us?
		mr0 = 548;
		mr1 = 576;
		break;

	case MFM_6US:											// 6 us?
		mr0 = 404;
		mr1 = 432;
		break;

	case MFM_4US:											// 4 us?
	default:
		mr0 = 260;
		mr1 = 288;
		break;
	}

//	LPC_TMR16B1->MR0 = mr0;
//	LPC_TMR16B1->MR1 = mr1;

	if(mfmByteIndex == 4) {								// streamed all times from byte?
		mfmByteIndex = 0;

		if(inCount == 0) {									// nothing to stream?
			mfmByte	= gap[gapCnt];						// stream this GAP byte
			gapCnt++;

			if(gapCnt > 2) {									// we got only 3 gap byte times, go back to 0
				gapCnt = 0;
			}
		} else {														// got data to stream?
			BYTE hostByte;
			inBuffer_get(hostByte);						// get byte from host buffer

			// lower nibble == 0? it's a command from host. if we should turn on/off the write protect or disk change
			if((hostByte & 0x0f) == 0) {			
				processHostCommand(hostByte);
				
				getNextMfmTime();				
			} else { // this wasn't a command, just store it
				mfmByte = hostByte;
			}
		}
	}
}

void processHostCommand(BYTE hostByte)
{
	switch(hostByte) {
		case CMD_WRITE_PROTECT_OFF:		GPIOB->BSRR	= WR_PROTECT;		break;			// WR PROTECT to 1
		case CMD_WRITE_PROTECT_ON:		GPIOB->BRR	= WR_PROTECT;		break;			// WR PROTECT to 0
		case CMD_DISK_CHANGE_OFF:			GPIOB->BSRR	= DISK_CHANGE;	break;			// DISK_CHANGE to 1
		case CMD_DISK_CHANGE_ON:			GPIOB->BRR	= DISK_CHANGE;	break;			// DISK_CHANGE to 0
		
		case CMD_CURRENT_SECTOR:								// get the next byte, which is sector #, and store it in sector variable
			inBuffer_get(sector);				
			addAttention(CMD_SEND_NEXT_SECTOR);		// also ask for the next sector to this one
			break;			
	}
}

void addAttention(BYTE atn)
{
	BYTE val;
	
	val = track;													// create 2nd byte: track + side
	if(side == 1) {
		val = val | 0x80;
	}
	
	outBuffer_add(atn);										// 1st byte: ATN code
	outBuffer_add(val);										// 2nd byte: side (highest bit) + track #
	outBuffer_add(sector);								// 3rd byte: current sector #
	
	GPIOB->BSRR = ATN;										// ATTENTION bit high - got something to read
}

void spi_init(void)
{
	SPI_InitTypeDef spiStruct;

	SPI_Cmd(SPI1, DISABLE);
	
	SPI_StructInit(&spiStruct);
	spiStruct.SPI_DataSize = SPI_DataSize_16b;		// use 16b data size to lower the MCU load
	
  SPI_Init(SPI1, &spiStruct);
	
	SPI_Cmd(SPI1, ENABLE);
}

void spi_TxRx(void)
{
	WORD status = SPI1->SR;									// read SPI status
	
	if(status & (1 << 1)) {									// TXE flag: Tx buffer empty
		if(outCount >= 2) {										// something to send? good
			WORD hi, lo;
			outBuffer_get(hi);									// get from outbuffer twice
			outBuffer_get(lo);
			hi = (hi << 8) | lo;								// combine bytes into word
			
			SPI1->DR = hi;											// send over SPI
		} else {															// no data to send, just send zero
			GPIOB->BRR = ATN;										// ATTENTION bit low - nothing to read	
			
			SPI1->DR = 0;
		}
	}
	
	if(status & (1 << 0)) {									// RXNE flag: Rx buffer not empty
		WORD data = SPI1->DR;									// get data
		inBuffer_add(data >> 8);
		inBuffer_add(data & 0xff);
	}
}
