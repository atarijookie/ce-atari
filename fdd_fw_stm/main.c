#include "stm32f10x.h"                       /* STM32F103 definitions         */
#include "stm32f10x_spi.h"
#include "stm32f10x_tim.h"

#include "misc.h"

#include "defs.h"
#include "timers.h"

void initSpi(void);

void mfm_append_change(BYTE change);
void getNextMfmTime(void);
void addAttention(BYTE atn);
void spi_TxRx(void);


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

#define CMD_TRACK_CHANGED       1               // sent: 1, side (highest bit) + track #, current sector #
#define CMD_SEND_NEXT_SECTOR    2               // sent: 2, side (highest bit) + track #, current sector #
#define CMD_SECTOR_WRITTEN      3               // sent: 3, side (highest bit) + track #, current sector #

#define MFM_4US     1
#define MFM_6US     2
#define MFM_8US     3

// - check a obnova rdata timeru musi zbehnut do 260 cyklov (tolko je pre 4 us) -- of jedneho po druhe volanie getNextMfmTime()
//     -- trvania: d4, a9, bd, e0 -- bez SPI. Mozno pridat ten getNextMfmTime na viacere miesta na znizenie trvania checku.
// - write support

// set GPIOB as --- CNF1:0 -- 00 (push-pull output), MODE1:0 -- 11 (output 50 Mhz)
#define 	FloppyOut_Enable()			{GPIOB->CRH &= ~(0x00000fff); GPIOB->CRH |=  (0x00000333); FloppyIndex_Enable(); }
// set GPIOB as --- CNF1:0 -- 01 (floating input), MODE1:0 -- 00 (input)
#define 	FloppyOut_Disable()			{GPIOB->CRH &= ~(0x00000fff); GPIOB->CRH |=  (0x00000444); FloppyIndex_Disable(); }

// enable / disable TIM1 CH1 output on GPIOA_8
#define		FloppyIndex_Enable()		{ GPIOA->CRH &= ~(0x0000000f); GPIOA->CRH |= (0x0000000b); };
#define		FloppyIndex_Disable()		{ GPIOA->CRH &= ~(0x0000000f); GPIOA->CRH |= (0x00000004); };

/*
void TIM1_UP_IRQHandler (void) {

  if ((TIM1->SR & 0x0001) != 0) {                 // check interrupt source

 		if(GPIOA->ODR & (1 << 10)) {
			GPIOA->BSRR = (1 << 26);
		} else {
			GPIOA->BSRR = (1 << 10);
		}

    TIM1->SR &= ~(1<<0);                          // clear UIF flag
 }
}
*/

int main (void) 
{
	BYTE prevSide, outputsActive;
// NVIC_InitTypeDef Init;
	
  RCC->APB2ENR |= (1 << 12) | (1 << 11) | (1 << 3) | (1 << 2);     			// Enable SPI1, TIM1, GPIOA and GPIOB clock

	// set FLOATING INPUTs for GPIOB_0 ... 6
	GPIOB->CRL &= ~(0x0fffffff);						// remove bits from GPIOB
	GPIOB->CRL |=   0x04444444;							// set GPIOB as --- CNF1:0 -- 01 (floating input), MODE1:0 -- 00 (input), PxODR -- don't care

	FloppyOut_Disable();
	
	// enable atlernate function for PA4, PA5, PA6, PA7 == SPI
	GPIOA->CRL &= ~(0xffff0000);						// remove bits from GPIOA
	GPIOA->CRL |=   0xbbbb0000;							// set GPIOA as --- CNF1:0 -- 10 (push-pull), MODE1:0 -- 11, PxODR -- don't care

	
	
	//	// config PIO3 outputs
	//	WORD *pio3dir = (WORD *) 0x50038000;
	//	*pio3dir = ATN;


	RCC->APB2ENR |= (1 << 0);									// enable AFIO
	AFIO->EXTICR[0] = 0x1000;									// EXTI3 -- source input: GPIOB_3
	EXTI->IMR			= STEP;											// EXTO3 -- 1 means: Interrupt from line 3 not masked
	EXTI->EMR			= STEP;											// EXTO3 -- 1 means: Event     form line 3 not masked
	EXTI->FTSR 		= STEP;											// Falling trigger selection register - STEP pulse
	

	//----------

	timerSetup_index();
	TIM_ITConfig(TIM1, TIM_IT_Update, ENABLE);

/*
//  Init.NVIC_IRQChannel = TIM1_IRQn;
	Init.NVIC_IRQChannel = TIM1_UP_IRQn;
	Init.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&Init);
*/

	initSpi();																		// init SPI interface

	while(1) {
		
	}
/*	
		WORD val = TIM1->SR;
		
		if((val & 0x0001) == 0) {			// no overflow? continue
			continue;
		}
		
		TIM1->SR = val & 0xfffe;			// clear UIF flag
		
//		if(TIM1->CNT == 0) {
			
		if(GPIOA->ODR & (1 << 10)) {
			GPIOA->BSRR = (1 << 26);
		} else {
			GPIOA->BSRR = (1 << 10);
		}
			
//			while(TIM1->CNT == 0);
//		}
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
	sector				= 0;
	prevSide			= 0;
	outputsActive	= 0;

	// init floppy signals
	GPIOB->BSRR = (WR_PROTECT | DISK_CHANGE);			// not write protected, not changing disk (ready!)

	while(1) {
		WORD ints;
		WORD inputs = GPIOB->IDR;								// read floppy inputs

		spi_TxRx();															// 0.5 us per received byte

		if((inputs & (MOTOR_ENABLE | DRIVE_SELECT)) != 0) {		// motor not enabled, drive not selected? do nothing
			if(outputsActive == 1) {							// if we got outputs active
				FloppyOut_Disable();								// all pins as inputs, drive not selected
				outputsActive = 0;
			}

			continue;
		}

		if(outputsActive == 0) {								// if the outputs are inactive
			FloppyOut_Enable();										// set these as outputs
			outputsActive = 1;
		}

		
//		WORD tmr1Ints = LPC_TMR16B1->IR;						// read TMR16B1, which we to create MFM stream
//		if(tmr1Ints & 2) {										// is MR1INT set? we streamed out one time
		{
//			LPC_TMR16B1->IR = 2;								// clear MR1INT
			getNextMfmTime();									// get the next time from stream, set up TMR16B1
		}

		// check for STEP pulse - should we go to a different track?
		ints = EXTI->PR;										// Pending register (EXTI_PR)
		if(ints & STEP) {										// if falling edge of STEP signal was found
			EXTI->PR = STEP;									// clear that int

			if(inputs & DIR) {								// direction is High? track--
				if(track > 0) {
					track--;

					addAttention(CMD_TRACK_CHANGED);
				}
			} else {											// direction is Low? track++
				if(track < 82) {
					track++;

					addAttention(CMD_TRACK_CHANGED);
				}
			}

			if(track == 0) {									// if track is 0
				GPIOB->BSRR = (TRACK0 << 16);		// TRACK 0 signal to L			(write bit 26)
			} else {													// if track is not 0
				GPIOB->BSRR = TRACK0;						// TRACK 0 signal is H			(write bit 10)
			}
		}

		//------------
		// update SIDE var
		side = (inputs & SIDE1) ? 0 : 1;						// get the current SIDE
		if(prevSide != side) {									// side changed?
			addAttention(CMD_SEND_NEXT_SECTOR);					// we need another sector, this time from the right side!
			prevSide = side;
		}

		//------------
		// check INDEX pulse as needed
		// if((TIM1->SR & 0x0001) != 0) {		// overflow of TIM1 occured?
		// TIM1->SR &= 0xfffe;							// clear UIF flag
		{

			// TODO: TRACK started! init code needed
			addAttention(CMD_TRACK_CHANGED);

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
			hostByte = inBuffer[inIndexGet];	// get byte from host buffer
			inIndexGet++;											// increment position for reading in host buffer
			inCount--;												// decrement data in cyclic buffer

			// TODO: add checking and handling of some commands from host (e.g. media change)


			mfmByte = hostByte;
		}
	}
}

void addAttention(BYTE atn)
{
	BYTE val;
	
	//TODO: signal to host, send command to host

	outBuffer[outIndexAdd] = atn;					// 1st byte: ATN code
	outIndexAdd++;												// update index for adding data
	outIndexAdd = outIndexGet & 0x7ff;		// limit to 2048 bytes

	//-----
	val = track;													// create 2nd byte: track + side
	if(side == 1) {
		val = val | 0x80;
	}
	
	outBuffer[outIndexAdd] = val;					// 2nd byte: side (highest bit) + track #
	outIndexAdd++;												// update index for adding data
	outIndexAdd = outIndexGet & 0x7ff;		// limit to 2048 bytes

	//-----
	outBuffer[outIndexAdd] = sector;			// 3rd byte: current sector #
	outIndexAdd++;												// update index for adding data
	outIndexAdd = outIndexGet & 0x7ff;		// limit to 2048 bytes
	
	outCount += 3;												// update count
}

void initSpi(void)
{
	SPI_InitTypeDef spiStruct;

	SPI_Cmd(SPI1, DISABLE);
	
	SPI_StructInit(&spiStruct);
  SPI_Init(SPI1, &spiStruct);
	
	SPI_Cmd(SPI1, ENABLE);
}

void spi_TxRx(void)
{
	WORD status = SPI1->SR;
	
	if(status & (1 << 1)) {									// TXE flag: Tx buffer empty
		if(outCount > 0) {										// something to send? good
			SPI1->DR = outBuffer[outIndexGet];
			
			outIndexGet++;
			outIndexGet = outIndexGet & 0x7ff;	// limit to 2048 bytes
			
			outCount--;
		} else {															// no data to send, just send zero
			SPI1->DR = 0;
		}
	}
	
	if(status & (1 << 0)) {								// RXNE flag: Rx buffer not empty
		inBuffer[inIndexAdd] = SPI1->DR;		// get data
		
		inIndexAdd++;												// update index for adding data
		inIndexAdd = inIndexAdd & 0xfff;		// limit to 4096 bytes
		
		inCount++;													// update count
	}
}
