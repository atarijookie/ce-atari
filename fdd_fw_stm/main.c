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


// worst case scenario: 1 sector with all bullshit encoded in MFM should be max. 1228 bytes
// So a 4096 bytes big buffer should contain at least 3.3 sectors (one currently streamed, one received from host + something more)

// gap byte 0x4e == 666446 us = 222112 times = 10'10'10'01'01'10 = 0xa96
// 2 gap bytes 0x4e 0x4e = 0xa96a96 times = 0xa9 0x6a 0x96 bytes

// first gap (60 * 0x4e) takes 1920 us (1.9 ms)

BYTE mfm[4096];
WORD mfmIndexAdd, mfmIndexGet, mfmCount;
BYTE mfmByte, mfmByteIndex;

BYTE newByte;
BYTE newBits;

BYTE changes;
BYTE chCount;

DWORD sync;
BYTE scnt;

#define CMD_TRACK_CHANGED       1               // +  track # + side (highest bit)
#define CMD_SEND_NEXT_SECTOR    2               // +  track # + side (highest bit)
#define CMD_SECTOR_WRITTEN      3               // + sector # + side (highest bit)
#define CMD_BUTTON_PRESSED      4

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



// enable TIM1 CH1 output on GPIOA_8
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
	BYTE track = 0;
	BYTE side, prevSide = 0;
	BYTE outputsActive = 0;
// NVIC_InitTypeDef Init;
	
  RCC->APB2ENR |= (1 << 12) | (1 << 11) | (1 << 3) | (1 << 2);     			// Enable SPI1, TIM1, GPIOA and GPIOB clock

	// set FLOATING INPUTs for GPIOB_0 ... 6
	GPIOB->CRL &= ~(0x0fffffff);						// remove bits from GPIOB
	GPIOB->CRL |=   0x04444444;							// set GPIOB as --- CNF1:0 -- 01 (floating input), MODE1:0 -- 00 (input), PxODR -- don't care

	FloppyOut_Disable();
	
	// enable atlernate function for PA4, PA5, PA6, PA7 == SPI
	GPIOA->CRL &= ~(0xffff0000);						// remove bits from GPIOA
	GPIOA->CRL |=   0xbbbb0000;							// set GPIOA as --- CNF1:0 -- 10 (push-pull), MODE1:0 -- 11, PxODR -- don't care


	// config PIO2 outputs
//	WORD *pio2dir = (WORD *) 0x50028000;
//	*pio2dir = 0;							// all pins as inputs, drive not selected

	//	// config PIO3 outputs
	//	WORD *pio3dir = (WORD *) 0x50038000;
	//	*pio3dir = ATN;

	// GPIO3IS  is 0 after reset = edge sensitive
	// GPIO3IEV is 0 after reset = interrupt on falling edge
	//	WORD *GPIO3RIS = (WORD *) 0x50038014;		// GPIO raw interrupt status register -- read to get interrupt status
	//	WORD *GPIO3IC  = (WORD *) 0x5003801C;		// GPIO interrupt clear register      -- write 1 to clear interrupt bit

//	WORD *GPIO2RIS	= (WORD *) 0x50028014;		// GPIO raw interrupt status register -- read to get interrupt status
//	WORD *GPIO2IC	= (WORD *) 0x5002801C;		// GPIO interrupt clear register      -- write 1 to clear interrupt bit

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

	// init circular buffer for streamed data
	mfmIndexAdd		= 0;
	mfmIndexGet		= 0;
	mfmCount			= 0;

	mfmByte				= 0;
	mfmByteIndex	= 0;

	// init track and side vars for the floppy position



	// init floppy signals
	GPIOB->BSRR = (WR_PROTECT | DISK_CHANGE);			// not write protected, not changing disk (ready!)

	while(1) {
		WORD inputs = GPIOB->IDR;															// read floppy inputs

		// TODO: add reading of SPI here
		

		if((inputs & (MOTOR_ENABLE | DRIVE_SELECT)) != 0) {		// motor not enabled, drive not selected? do nothing
			if(outputsActive == 1) {							// if we got outputs active
				FloppyOut_Disable();								// all pins as inputs, drive not selected
				outputsActive = 0;
			}

			continue;
		}

		if(outputsActive == 0) {								// if the outputs are inactive
			FloppyOut_Disable();									// set these as outputs
			outputsActive = 1;
		}

		
//		WORD tmr1Ints = LPC_TMR16B1->IR;						// read TMR16B1, which we to create MFM stream
//		if(tmr1Ints & 2) {										// is MR1INT set? we streamed out one time
		{
//			LPC_TMR16B1->IR = 2;								// clear MR1INT
			getNextMfmTime();									// get the next time from stream, set up TMR16B1
		}

		
		// check for STEP pulse - should we go to a different track?
//		WORD ints = *GPIO2RIS;									// read interrupt status of GPIO2
//		if(ints & STEP) {										// if falling edge of STEP signal was found
		{
//			*GPIO2IC = STEP;									// clear that int

			if(inputs & DIR) {									// direction is High? track--
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

	if(mfmByteIndex == 4) {										// streamed all times from byte?
		mfmByteIndex = 0;

		if(mfmCount == 0) {										// nothing to stream?
			mfmByte	= gap[gapCnt];								// stream this GAP byte
			gapCnt++;

			if(gapCnt > 2) {									// we got only 3 gap byte times, go back to 0
				gapCnt = 0;
			}
		} else {												// got data to stream?
			BYTE hostByte;
			hostByte = mfm[mfmIndexGet];						// get byte from host buffer
			mfmIndexGet++;										// increment position for reading in host buffer
			mfmCount--;											// decrement data in cyclic buffer

			// TODO: add checking and handling of some commands from host (e.g. media change)


			mfmByte = hostByte;
		}
	}
}

void addAttention(BYTE atn)
{
	//TODO: signal to host, send command to host


}

void initSpi(void)
{
	SPI_InitTypeDef spiStruct;

	SPI_Cmd(SPI1, DISABLE);
	
	SPI_StructInit(&spiStruct);
  SPI_Init(SPI1, &spiStruct);
	
	SPI_Cmd(SPI1, ENABLE);
}
