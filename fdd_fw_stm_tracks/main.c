#include "stm32f10x.h"                       // STM32F103 definitions
#include "stm32f10x_spi.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_dma.h"
#include "stm32f10x_exti.h"
#include "misc.h"

#include "defs.h"
#include "timers.h"

void init_hw_sw(void);

void fillMfmTimesForDMA(void);
BYTE getNextMFMword(void);

void getMfmWriteTimes(void);

void processHostCommand(WORD hostWord);
void requestTrack(void);

void spi_init(void);

void dma_mfmRead_init(void);
void dma_mfmWrite_init(void);
void dma_spi_init(void);

WORD spi_TxRx(WORD out);
void spiDma_txRx(WORD txCount, BYTE *txBfr, WORD rxCount, BYTE *rxBfr);

/*
TODO:
 - POZOR! STEP moze robit / robi ked neni drive selected, ale len MOTOR ON
 - medzi STEP je 3 ms
 - az ked doSTEPuje, tak zleze dole drive select
 - pomeranie ci stihne zachytit vsetky STEP pulzy ak naplna MFM buffre
 - pomerat ci a kolko pauzy robi floppy po seeku, dali by sa tym osetrit viacere seeky po sebe
 - test TIM3 input capture bez DMA, potom s DMA
 - pomeranie kolko trvaju jednotlive casti kodu
*/

// 1 sector with all headers, gaps, data and crc: 614 B of data needed to stream -> 4912 bits to stream -> with 4 bits encoded to 1 byte: 1228 of encoded data
// Each sector takes around 22ms to stream out, so you have this time to ask for new one and get it in... the SPI transfer of 1228 B should take 0,8 ms.
// inBuffer size of 2048 WORDs can contain 3.3 encoded sectors

// gap byte 0x4e == 666446 us = 222112 times = 10'10'10'01'01'10 = 0xa96
// first gap (60 * 0x4e) takes 1920 us (1.9 ms)

// This ARM is little endian, e.g. 0x1234 is stored in RAM as 34 12, but when working with WORDs, it's OK

// maximum params from .ST images seem to be: 84 tracks (0-83), 10 sectors/track

#define INBUFFER_SIZE					7500
WORD inBuffer[INBUFFER_SIZE];
WORD inIndexGet;

TWriteBuffer wrBuffer[2];							// two buffers for written sectors
TWriteBuffer *wrNow;

BYTE side, track;

struct {
	BYTE side;
	BYTE track;
	BYTE sector;
} streamed;

WORD mfmReadStreamBuffer[16];							// 16 words - 16 mfm times. Half of buffer is 8 times - at least 32 us (8 * 4us),

WORD mfmWriteStreamBuffer[16];
WORD lastMfmWriteTC;

// cycle measure: t1 = TIM3->CNT;	t2 = TIM3->CNT;	dt = t2 - t1; -- subtrack 0x12 because that's how much measuring takes
WORD t1, t2, dt; 

/* Franz to host communication:
A) send   : ATN_SEND_TRACK with the track # and side # -- 2 WORDs + zeros = 3 WORDs
   receive: track data with sector start marks, up to 15 kB -- 12 sectors + the marks

B) send   : ATN_SECTOR_WRITTEN with the track, side, sector # + captured data, up to 1500 B
   receive: nothing (or don't care)
	 
C) send   : ATN_FW_VERSION with the FW version + empty bytes == 3 WORD for FW + empty WORDs
   receive: commands possibly received -- receive 6 WORDs (3 empty, 3 with possible commands)
*/

// commands sent from device to host
#define ATN_FW_VERSION					0x01								// followed by string with FW version (length: 4 WORDs - cmd, v[0], v[1], 0)
//#define ATN_SEND_NEXT_SECTOR    0x02              // sent: 2, side, track #, current sector #, 0, 0, 0, 0 (length: 4 WORDs)
#define ATN_SECTOR_WRITTEN      0x03               	// sent: 3, side (highest bit) + track #, current sector #
#define ATN_SEND_TRACK          0x04            		// send the whole track


// commands sent from host to device
#define CMD_WRITE_PROTECT_OFF		0x10
#define CMD_WRITE_PROTECT_ON		0x20
#define CMD_DISK_CHANGE_OFF			0x30
#define CMD_DISK_CHANGE_ON			0x40
#define CMD_CURRENT_SECTOR			0x50								// followed by sector #
#define CMD_GET_FW_VERSION			0x60
#define CMD_SET_DRIVE_ID_0			0x70
#define CMD_SET_DRIVE_ID_1			0x80
#define CMD_CURRENT_TRACK       0x90                // followed by track #
#define CMD_MARK_READ						0xF000							// this is not sent from host, but just a mark that this WORD has been read and you shouldn't continue to read further
#define CMD_TRACK_STREAM_END		0xF000							// this is the mark in the track stream that we shouldn't go any further in the stream

#define MFM_4US     1
#define MFM_6US     2
#define MFM_8US     3

// set GPIOB as --- CNF1:0 -- 00 (push-pull output), MODE1:0 -- 11 (output 50 Mhz)
//#define 	FloppyOut_Enable()			{GPIOB->CRH &= ~(0x00000fff); GPIOB->CRH |=  (0x00000333); FloppyIndex_Enable();	FloppyMFMread_Enable();		}
#define 	FloppyOut_Enable()			{GPIOB->CRH &= ~(0x00000fff); GPIOB->CRH |=  (0x00000737); FloppyIndex_Enable();	FloppyMFMread_Enable();		}
// set GPIOB as --- CNF1:0 -- 01 (floating input), MODE1:0 -- 00 (input)
#define 	FloppyOut_Disable()			{GPIOB->CRH &= ~(0x00000fff); GPIOB->CRH |=  (0x00000444); FloppyIndex_Disable();	FloppyMFMread_Disable();	}

// enable / disable TIM1 CH1 output on GPIOA_8
// open drain
#define		FloppyMFMread_Enable()		{ GPIOA->CRH &= ~(0x0000000f); GPIOA->CRH |= (0x0000000f); };
// push pull
//#define		FloppyMFMread_Enable()		{ GPIOA->CRH &= ~(0x0000000f); GPIOA->CRH |= (0x0000000b); };
#define		FloppyMFMread_Disable()		{ GPIOA->CRH &= ~(0x0000000f); GPIOA->CRH |= (0x00000004); };

// enable / disable TIM2 CH4 output on GPIOB_11
#define		FloppyIndex_Enable()			{ GPIOB->CRH &= ~(0x0000f000); GPIOB->CRH |= (0x0000f000); };
//#define		FloppyIndex_Enable()			{ GPIOB->CRH &= ~(0x0000f000); GPIOB->CRH |= (0x0000b000); };
#define		FloppyIndex_Disable()			{ GPIOB->CRH &= ~(0x0000f000); GPIOB->CRH |= (0x00004000); };

//--------------
// circular buffer 
// watch out, these macros take 0.73 us for _add, and 0.83 us for _get operation!

#define		wrBuffer_add(X)					{ if(wrNow->count  < 550)	{		wrNow->buffer[wrNow->count]		= X;	wrNow->count++;		}	}

#define		inBuffer_goToStart()				{ 																				inIndexGet = 0;																		}
#define		inBuffer_get(X)							{ X = inBuffer[inIndexGet];								inIndexGet++;		if(inIndexGet >= INBUFFER_SIZE) { inIndexGet = 0; }; }
#define		inBuffer_get_noMove(X)			{ X = inBuffer[inIndexGet];																																	}
#define		inBuffer_markAndmove()			{ inBuffer[inIndexGet] = CMD_MARK_READ;		inIndexGet++;		if(inIndexGet >= INBUFFER_SIZE) { inIndexGet = 0; };	}
#define		inBuffer_justMove()					{ 																				inIndexGet++;		if(inIndexGet >= INBUFFER_SIZE) { inIndexGet = 0; };	}
//--------------
WORD version[2] = {0xf013, 0x0718};				// this means: Franz, 2013-07-18
WORD drive_select;

volatile BYTE sendFwVersion, sendTrackRequest;
WORD atnSendFwVersion[5], atnSendTrackRequest[4], cmdBuffer[6];
WORD fakeBuffer;

volatile struct {
	BYTE track;
	BYTE side;
} next;

int main (void) 
{
	BYTE prevSide = 0, outputsActive = 0;
	BYTE indexCount = 0;
	WORD prevWGate = WGATE, WGate;
	BYTE spiDmaIsIdle = TRUE;

WORD i, err=0;

	next.track	= 0;
	next.side		= 0;
	
	sendFwVersion			= FALSE;
	sendTrackRequest	= FALSE;
	
	atnSendFwVersion[0] = 0;											// just-in-case padding
	atnSendFwVersion[1] = ATN_FW_VERSION;					// attention code
	atnSendFwVersion[2] = version[0];
	atnSendFwVersion[3] = version[1];
	atnSendFwVersion[4] = 0;											// terminating zero
	
	atnSendTrackRequest[0] = 0;										// just-in-case padding
	atnSendTrackRequest[1] = ATN_SEND_TRACK;			// attention code
	atnSendTrackRequest[3] = 0;										// terminating zero
	
	init_hw_sw();																	// init GPIO pins, timers, DMA, global variables

	drive_select = MOTOR_ENABLE | DRIVE_SELECT0;	// the drive select bits which should be LOW if the drive is selected

	// init floppy signals
	GPIOB->BSRR = (WR_PROTECT | DISK_CHANGE);			// not write protected
	GPIOB->BRR = TRACK0 | DISK_CHANGE;						// TRACK 0 signal to L, DISK_CHANGE to LOW		
	GPIOB->BRR = ATN;															// ATTENTION bit low - nothing to read
	
	requestTrack();																// request track 0, side 0

/*
FloppyOut_Enable();
track = 0;
while(1);
*/
/*
{
		// check for STEP pulse - should we go to a different track?
		i = EXTI->PR;										// Pending register (EXTI_PR)
		err = GPIOB->IDR;										// read floppy inputs

		if(i & STEP) {										// if falling edge of STEP signal was found
			EXTI->PR = STEP;									// clear that int
			if((err & DIR) != 0) {								// direction is High? track--
				if(track > 0) {
					track--;

					inBuffer[inIndexGet] = track;
					inIndexGet++;
				}
			} else {											// direction is Low? track++
				if(track < 85) {
					track++;

					inBuffer[inIndexGet] = track;
					inIndexGet++;
				}
			}

			if(track == 0) {									// if track is 0
				GPIOB->BRR = TRACK0;						// TRACK 0 signal to L			
			} else {													// if track is not 0
				GPIOB->BSRR = TRACK0;						// TRACK 0 signal is H
			}
		}	
}
*/

	while(1) {
		WORD ints, inputs;
		
		// sending and receiving data over SPI using DMA
		if((DMA1->ISR & (DMA1_IT_TC3 | DMA1_IT_TC2)) == (DMA1_IT_TC3 | DMA1_IT_TC2)) {	// SPI DMA: nothing to Tx and nothing to Rx?
			DMA1->IFCR = DMA1_IT_TC3 | DMA1_IT_TC2;																				// clear HTIF flags
			spiDmaIsIdle	= TRUE;																	// mark that the SPI DMA is ready to do something
		}

		if(spiDmaIsIdle == TRUE) {															// SPI DMA: nothing to Tx and nothing to Rx?
			if(sendFwVersion) {																		// should send FW version? this is a window for receiving commands
				spiDma_txRx(5, (BYTE *) &atnSendFwVersion[0], 6, (BYTE *) &cmdBuffer[0]);
				spiDmaIsIdle			= FALSE;													// SPI DMA is busy
				
				sendFwVersion			= FALSE;
			} else if(sendTrackRequest) {
				atnSendTrackRequest[2] = (((WORD)next.side) << 8) | (next.track);

				spiDma_txRx(4, (BYTE *) &atnSendTrackRequest[0], INBUFFER_SIZE, (BYTE *) &inBuffer[0]);
				spiDmaIsIdle			= FALSE;													// SPI DMA is busy
				
				sendTrackRequest	= FALSE;
			} else if(wrNow->readyToSend) {												// not sending any ATN right now? and current write buffer has something?
				spiDma_txRx(wrNow->count, (BYTE *) &wrNow->buffer[0], 1, (BYTE *) &fakeBuffer);
				spiDmaIsIdle	= FALSE;															// SPI DMA is busy

				wrNow->readyToSend	= FALSE;												// mark the current buffer as not ready to send (so we won't send this one again)
				
				wrNow								= wrNow->next;									// and now we will select the next buffer as current
				wrNow->readyToSend	= FALSE;												// the next buffer is not ready to send (yet)
				wrNow->count				= 0;														
			}
		}
		
		if(DMA1_Channel3->CNDTR != 0) {												// something to send over SPI?
			GPIOB->BSRR = ATN;																	// ATTENTION bit high - got something to read
		} else {
			GPIOB->BRR = ATN;																		// ATTENTION bit low  - nothing to read
		}				 
		
		//-------------------------------------------------
		// if we got something in the cmd buffer, we should process it
		if(spiDmaIsIdle == TRUE && cmdBuffer[0] != CMD_MARK_READ) {	// if we're not sending (receiving) and the cmd buffer is not read
			int i;
			
			for(i=0; i<6; i++) {
				processHostCommand(cmdBuffer[i]);
			}
			
			cmdBuffer[0] = CMD_MARK_READ;													// mark that this cmd buffer is already read
		}
		
		//-------------------------------------------------
		inputs = GPIOB->IDR;										// read floppy inputs

		// now check if the drive is ON or OFF and handle it
		if((inputs & drive_select) != 0) {			// motor not enabled, drive not selected? disable outputs
 			if(outputsActive == 1) {							// if we got outputs active
				FloppyOut_Disable();								// all pins as inputs, drive not selected
				outputsActive = 0;
			}
		} else {																// motor ON, drive selected
			if(outputsActive == 0) {							// if the outputs are inactive
				FloppyOut_Enable();									// set these as outputs
				outputsActive = 1;
			}
		}
		
		//-------------------------------------------------
/*		
		WGate = inputs & WGATE;												// get current WGATE value
		if(prevWGate != WGate) {											// if WGate changed
			
			if(WGate == 0) {														// on falling edge of WGATE
				WORD wval;

				lastMfmWriteTC = TIM3->CNT;								// set lastMfmWriteTC to current TC value on WRITE start
				wrNow->readyToSend = FALSE;								// mark this buffer as not ready to be sent yet
				
				wval = (ATN_SECTOR_WRITTEN << 8) | streamed.side;		// 1st byte: ATN code, 2nd byte: side 
				wrBuffer_add(wval);
	
				wval = (streamed.track << 8) | streamed.sector;			// 3rd byte: track #, 4th byte: current sector #
				wrBuffer_add(wval);									
			} else {																		// on rising edge
				wrBuffer_add(0);													// last word: 0
				
				wrNow->readyToSend = TRUE;								// mark this buffer as ready to be sent
			}			
			
			prevWGate = WGate;													// store WGATE value
		}
		
		if(WGate == 0) {																				// when write gate is low, the data is written to floppy
			if((DMA1->ISR & (DMA1_IT_TC6 | DMA1_IT_HT6)) != 0) {	// MFM write stream: TC or HT interrupt? we've streamed half of circular buffer!
				getMfmWriteTimes();
			}
		}
*/

		if((DMA1->ISR & (DMA1_IT_TC5 | DMA1_IT_HT5)) != 0) {		// MFM read stream: TC or HT interrupt? we've streamed half of circular buffer!
			fillMfmTimesForDMA();																	// fill the circular DMA buffer with mfm times
		}

		//------------
		// check INDEX pulse as needed -- duration when filling with marks: 69.8 us
		if((TIM2->SR & 0x0001) != 0) {		// overflow of TIM1 occured?
			int i;
			TIM2->SR = 0xfffe;							// clear UIF flag
	
			inBuffer_goToStart();						// move the pointer in the track stream to start
			
			for(i=0; i<16; i++) {						// copy 'all 4 us' pulses into current streaming buffer to allow shortest possible switch to start of track
				mfmReadStreamBuffer[i] = 7;
			}
			
			// the following few lines send the FW version to host every 5 index pulses, this is used for transfer of commands from host to Franz
			indexCount++;
			
			if(indexCount == 5) {
				indexCount = 0;
				sendFwVersion = TRUE;
			}
		}
		
		//--------
		// NOTE! Handling of STEP and SIDE only when MOTOR is ON, but the drive doesn't have to be selected and it must handle the control anyway
		if((inputs & MOTOR_ENABLE) != 0) {			// motor not enabled? Skip the following code.
			continue;
		}

		/*
		// check for STEP pulse - should we go to a different track?
		ints = EXTI->PR;										// Pending register (EXTI_PR)
		
		if(ints & STEP) {										// if falling edge of STEP signal was found
			EXTI->PR = STEP;									// clear that int
			if(inputs & DIR) {								// direction is High? track--
				if(track > 0) {
					track--;

					requestTrack();
				}
			} else {											// direction is Low? track++
				if(track < 85) {
					track++;

					requestTrack();
				}
			}

			if(track == 0) {									// if track is 0
				GPIOB->BRR = TRACK0;						// TRACK 0 signal to L			
			} else {													// if track is not 0
				GPIOB->BSRR = TRACK0;						// TRACK 0 signal is H
			}
		}
*/
		//------------
		// update SIDE var
		side = (inputs & SIDE1) ? 0 : 1;					// get the current SIDE
		if(prevSide != side) {										// side changed?
			requestTrack();										// we need track from the right side
			prevSide = side;
		}

	}
}

void spiDma_txRx(WORD txCount, BYTE *txBfr, WORD rxCount, BYTE *rxBfr)
{
	// disable both TX and RX channels
	DMA1_Channel3->CCR		&= 0xfffffffe;								// disable DMA1 Channel transfer
	DMA1_Channel2->CCR		&= 0xfffffffe;								// disable DMA1 Channel transfer

	// config SPI1_TX -- DMA1_CH3
	DMA1_Channel3->CMAR		= (uint32_t) txBfr;						// from this buffer located in memory
	DMA1_Channel3->CNDTR	= txCount;										// this much data
	
	// config SPI1_RX -- DMA1_CH2
	DMA1_Channel2->CMAR		= (uint32_t) rxBfr;						// to this buffer located in memory
	DMA1_Channel2->CNDTR	= rxCount;										// this much data

	// enable both TX and RX channels
	DMA1_Channel3->CCR		|= 1;													// enable  DMA1 Channel transfer
	DMA1_Channel2->CCR		|= 1;													// enable  DMA1 Channel transfer

/*	
	if((SPI1->SR & 2) != 0) {														// TXE flag: Tx buffer empty
		SPI1->DR = 0;																			// just send zero over SPI
	}
*/
	
//  SPI1->CR1 |= (1 << 6);															// SPI enable
}

void init_hw_sw(void)
{
	WORD i;
	NVIC_InitTypeDef NVIC_InitStructure;
	
	RCC->AHBENR		|= (1 <<  0);																						// enable DMA1
	RCC->APB1ENR	|= (1 <<  1) | (1 <<  0);																// enable TIM3, TIM2
  RCC->APB2ENR	|= (1 << 12) | (1 << 11) | (1 << 3) | (1 << 2);     		// Enable SPI1, TIM1, GPIOA and GPIOB clock
	
	// set FLOATING INPUTs for GPIOB_0 ... 6
	GPIOB->CRL &= ~(0x0fffffff);						// remove bits from GPIOB
	GPIOB->CRL |=   0x04444444;							// set GPIOB as --- CNF1:0 -- 01 (floating input), MODE1:0 -- 00 (input), PxODR -- don't care

	FloppyOut_Disable();
	
	// SPI -- enable atlernate function for PA4, PA5, PA6, PA7
	GPIOA->CRL &= ~(0xffff0000);						// remove bits from GPIOA
	GPIOA->CRL |=   0xbbbb0000;							// set GPIOA as --- CNF1:0 -- 10 (push-pull), MODE1:0 -- 11, PxODR -- don't care

	// ATTENTION -- set GPIOB_15 (ATTENTION) as --- CNF1:0 -- 00 (push-pull output), MODE1:0 -- 11 (output 50 Mhz)
	GPIOB->CRH &= ~(0xf0000000); 
	GPIOB->CRH |=  (0x30000000);

	RCC->APB2ENR |= (1 << 0);									// enable AFIO
	
	AFIO->EXTICR[0] = 0x1000;									// EXTI3 -- source input: GPIOB_3
	EXTI->IMR			= STEP;											// EXTI3 -- 1 means: Interrupt from line 3 not masked
	EXTI->EMR			= STEP;											// EXTI3 -- 1 means: Event     form line 3 not masked
	EXTI->FTSR 		= STEP;											// Falling trigger selection register - STEP pulse
	
	/* Enable and set EXTI Line3 Interrupt to the lowest priority */
	
  NVIC_InitStructure.NVIC_IRQChannel = EXTI3_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);
	
	//----------
	AFIO->MAPR |= 0x02000000;									// SWJ_CFG[2:0] (Bits 26:24) -- 010: JTAG-DP Disabled and SW-DP Enabled
	AFIO->MAPR |= 0x00000300;									// TIM2_REMAP -- Full remap (CH1/ETR/PA15, CH2/PB3, CH3/PB10, CH4/PB11)
	AFIO->MAPR |= 0x00000800;									// TIM3_REMAP -- Partial remap (CH1/PB4, CH2/PB5, CH3/PB0, CH4/PB1)
	//----------
	
	timerSetup_index();
	
	timerSetup_mfmWrite();
	dma_mfmWrite_init();

	//--------------
	// DMA + SPI initialization
	for(i=0; i<INBUFFER_SIZE; i++) {							// fill the inBuffer with CMD_MARK_READ, which will tell that every byte is empty (already read)
		inBuffer[i] = CMD_MARK_READ;
	}
	
	spi_init();																		// init SPI interface
	dma_spi_init();

	// now init both writeBuffers
	wrBuffer[0].count				= 0;
	wrBuffer[0].readyToSend	= FALSE;
	wrBuffer[0].next				= &wrBuffer[1];
	
	wrBuffer[1].count				= 0;
	wrBuffer[1].readyToSend	= FALSE;
	wrBuffer[1].next				= &wrBuffer[0];

	wrNow = &wrBuffer[0];
	//--------------
	// configure MFM read stream by TIM1_CH1 and DMA in circular mode
	// WARNING!!! Never let mfmReadStreamBuffer[] contain a 0! With 0 the timer update never comes and streaming stops!
	for(i=0; i<16; i++) {
		mfmReadStreamBuffer[i] = 7;				// by default -- all pulses 4 us
	}
	
	timerSetup_mfmRead();								// init MFM READ stream timer
	dma_mfmRead_init();									// and init the DMA which will feed it from circular buffer
	//--------------
	
	// init circular buffer for data incomming via SPI
	inIndexGet		= 0;
	
	// init track and side vars for the floppy position
	side					= 0;
	track 				= 0;
	
	streamed.side		= 0;
	streamed.track	= 0;
	streamed.sector	= 1;
	
	lastMfmWriteTC = 0;
}

void EXTI3_IRQHandler(void)
{
  if(EXTI_GetITStatus(EXTI_Line3) != RESET)
  {
		WORD inputs = GPIOB->IDR;
		
		if((inputs & MOTOR_ENABLE) != 0) {			// motor not enabled? Skip the following code.
			EXTI_ClearITPendingBit(EXTI_Line3);		// Clear the EXTI line pending bit 
			return;
		}
		
		if(inputs & DIR) {								// direction is High? track--
			if(track > 0) {
				track--;

				next.track	= track;
				next.side		= side;
				sendTrackRequest = TRUE;
			}
		} else {													// direction is Low? track++
			if(track < 85) {
				track++;

				next.track	= track;
				next.side		= side;
				sendTrackRequest = TRUE;
			}
		}
		
		if(track == 0) {									// if track is 0
			GPIOB->BRR = TRACK0;						// TRACK 0 signal to L			
		} else {													// if track is not 0
			GPIOB->BSRR = TRACK0;						// TRACK 0 signal is H
		}
		
    // Clear the EXTI line pending bit 
    EXTI_ClearITPendingBit(EXTI_Line3);
  }
}


WORD spi_TxRx(WORD out)
{
	WORD in;

	while((SPI1->SR & (1 << 7)) != 0);				// TXE flag: BUSY flag

	while((SPI1->SR & 2) == 0);								// TXE flag: Tx buffer empty
	SPI1->DR = out;														// send over SPI

	while((SPI1->SR & 1) == 0);								// RXNE flag: RX buffer NOT empty
	in = SPI1->DR;														// get data
	
	return in;
}

void getMfmWriteTimes(void)
{
	WORD ind1, ind2, i, tmp, val, wval;
	
	// check for half transfer or transfer complete IF
	if((DMA1->ISR & DMA1_IT_HT6) != 0) {				// HTIF6 -- Half Transfer IF 6
		DMA1->IFCR = DMA1_IT_HT6;									// clear HTIF6 flag
		ind1 = 7;
		ind2 = 0;
	} else if((DMA1->ISR & DMA1_IT_TC6) != 0) {	// TCIF6 -- Transfer Complete IF 6
		DMA1->IFCR = DMA1_IT_TC6;									// clear TCIF6 flag
		ind1 = 15;
		ind2 = 8;
	}

	tmp = mfmWriteStreamBuffer[ind1];						// store the last one, we will move it to lastMfmWriteTC later
	
	for(i=0; i<7; i++) {												// create differences: this = this - previous; but only for 7 values
		mfmWriteStreamBuffer[ind1] = mfmWriteStreamBuffer[ind1] - mfmWriteStreamBuffer[ind1-1];
		ind1--;
	}

	mfmWriteStreamBuffer[ind1] = mfmWriteStreamBuffer[ind1] - lastMfmWriteTC;			// buffer[0] = buffer[0] - previousBuffer[15];  or buffer[8] = buffer[8] - previousBuffer[7];
	lastMfmWriteTC = tmp;																													// store current last item as last
	
	wval = 0;																		// init to zero
	
	for(i=0; i<8; i++) {												// now convert timer counter times to MFM times / codes
		tmp = mfmWriteStreamBuffer[ind2];
		ind2++;
		
		if(tmp < 200) {						// too short? skip it
			continue;
		} else if(tmp < 360) {		// 4 us?
			val = MFM_4US;
		} else if(tmp < 504) {		// 6 us?
			val = MFM_6US;
		} else if(tmp < 650) {		// 8 us?
			val = MFM_8US;
		} else {									// too long?
			continue;
		}
		
		wval = wval << 2;					// shift and add to the WORD 
		wval = wval | val;
	}
	
	wrBuffer_add(wval);					// add to write buffer
}

void fillMfmTimesForDMA(void)
{
	WORD ind = 0xff;

	// code to ARR value:      ??, 4us, 6us, 8us
	static WORD mfmTimes[4] = { 7,   7,  11,  15};
	BYTE time, i;
	WORD times8;

	// check for half transfer or transfer complete IF
	if((DMA1->ISR & DMA1_IT_HT5) != 0) {				// HTIF5 -- Half Transfer IF 5
		DMA1->IFCR = DMA1_IT_HT5;									// clear HTIF5 flag
		ind = 0;
	} else if((DMA1->ISR & DMA1_IT_TC5) != 0) {	// TCIF5 -- Transfer Complete IF 5
		DMA1->IFCR = DMA1_IT_TC5;									// clear TCIF5 flag
		ind = 8;
	}
	
	if(ind == 0xff) {														// no IF found? this shouldn't happen! did you come here without a reason?
		return;
	}
	
	times8 = getNextMFMword();									// get next word
	
	for(i=0; i<8; i++) {												// convert all 8 codes to ARR values
		time		= times8 >> 14;										// get bits 15,14 (and then 13,12 ... 1,0) 
		time		= mfmTimes[time];									// convert to ARR value
		times8	= times8 << 2;										// shift 2 bits higher so we would get lower bits next time

		mfmReadStreamBuffer[ind] = time;					// store and move to next one
		ind++;
	}
}

BYTE getNextMFMword(void)
{
	static WORD gap[3] = {0xa96a, 0x96a9, 0x6a96};
	static BYTE gapIndex = 0;
	WORD wval;

	WORD maxLoops = 15000;

	while(1) {														// go through inBuffer to process commands and to find some data
		maxLoops--;
		
		if(maxLoops == 0) {									// didn't quit the loop for 15k cycles? quit now!
			break;
		}
		
		inBuffer_get_noMove(wval);					// get WORD from buffer, but don't move
		
		if(wval == CMD_TRACK_STREAM_END) {	// we've hit the end of track stream? quit loop
			break;
		}
			
		inBuffer_justMove();								// just move to the next position
			
		if(wval == 0) {											// skip empty WORDs
			continue;
		}
		
		// lower nibble == 0? it's a command from host - if we should turn on/off the write protect or disk change
		if((wval & 0x0f00) == 0) {					// it's a command?
			processHostCommand(wval);
		} else {														// not a command? return it
			gapIndex = 0;		
			return wval;
		}
	}

	//---------
	// if we got here, we have no data to stream
	wval = gap[gapIndex];					// stream this GAP byte
	gapIndex++;

	if(gapIndex > 2) {						// we got only 3 gap WORD times, go back to 0
		gapIndex = 0;
	}

	return wval;
}

void processHostCommand(WORD hostWord)
{
	BYTE hi, lo;
	WORD nextWord;

	hi = hostWord >> 8;												// get upper byte
	lo = hostWord  & 0xff;										// get lower byte
	
	switch(hi) {
		case CMD_WRITE_PROTECT_OFF:		GPIOB->BSRR	= WR_PROTECT;		break;			// WR PROTECT to 1
		case CMD_WRITE_PROTECT_ON:		GPIOB->BRR	= WR_PROTECT;		break;			// WR PROTECT to 0
		case CMD_DISK_CHANGE_OFF:			GPIOB->BSRR	= DISK_CHANGE;	break;			// DISK_CHANGE to 1
		case CMD_DISK_CHANGE_ON:			GPIOB->BRR	= DISK_CHANGE;	break;			// DISK_CHANGE to 0

		case CMD_GET_FW_VERSION:			sendFwVersion = TRUE;				break;			// send FW version string and receive commands

		case CMD_CURRENT_SECTOR:								// read the current streamed side, track, sector numbers (will be used for write)
			streamed.side		= lo;									// store sector #
		
			inBuffer_get(nextWord);								// for the next ones we need to get another WORD from inBuffer
		
			streamed.track	= nextWord >> 8;			// store the numbers
			streamed.sector	= nextWord  & 0xff;
			break;			
		
		case CMD_SET_DRIVE_ID_0:			drive_select = MOTOR_ENABLE | DRIVE_SELECT0;		break;		// set drive ID pins to check like this...
		case CMD_SET_DRIVE_ID_1:			drive_select = MOTOR_ENABLE | DRIVE_SELECT1;		break;		// ...or that!
	}
}

void requestTrack(void)
{
	next.track	= track;
	next.side		= side;
	
	sendTrackRequest = TRUE;
}


void spi_init(void)
{
	SPI_InitTypeDef spiStruct;

	SPI_Cmd(SPI1, DISABLE);
	
	SPI_StructInit(&spiStruct);
	spiStruct.SPI_DataSize = SPI_DataSize_16b;		// use 16b data size to lower the MCU load
	
  SPI_Init(SPI1, &spiStruct);
	SPI1->CR2 |= (1 << 7) | (1 << 6) | SPI_I2S_DMAReq_Tx | SPI_I2S_DMAReq_Rx;		// enable TXEIE, RXNEIE, TXDMAEN, RXDMAEN
	
	SPI_Cmd(SPI1, ENABLE);
}

void dma_mfmRead_init(void)
{
	DMA_InitTypeDef DMA_InitStructure;
	
	// DMA1 channel5 configuration -- TIM1_UP (update)
  DMA_DeInit(DMA1_Channel5);
	
  DMA_InitStructure.DMA_MemoryBaseAddr			= (uint32_t) mfmReadStreamBuffer;				// from this buffer located in memory
  DMA_InitStructure.DMA_PeripheralBaseAddr	= (uint32_t) &(TIM1->DMAR);					// to this peripheral address
  DMA_InitStructure.DMA_DIR									= DMA_DIR_PeripheralDST;						// dir: from mem to periph
  DMA_InitStructure.DMA_BufferSize					= 16;																// 16 datas to transfer
  DMA_InitStructure.DMA_PeripheralInc				= DMA_PeripheralInc_Disable;				// PINC = 0 -- don't icrement, always write to DMAR register
  DMA_InitStructure.DMA_MemoryInc						= DMA_MemoryInc_Enable;							// MINC = 1 -- increment in memory -- go though buffer
  DMA_InitStructure.DMA_PeripheralDataSize	= DMA_PeripheralDataSize_HalfWord;	// each data item: 16 bits 
  DMA_InitStructure.DMA_MemoryDataSize			= DMA_MemoryDataSize_HalfWord;			// each data item: 16 bits 
  DMA_InitStructure.DMA_Mode								= DMA_Mode_Circular;								// circular mode
  DMA_InitStructure.DMA_Priority						= DMA_Priority_Medium;
  DMA_InitStructure.DMA_M2M									= DMA_M2M_Disable;									// M2M disabled, because we move to peripheral
  DMA_Init(DMA1_Channel5, &DMA_InitStructure);

  // Enable DMA1 Channel5 Transfer Complete interrupt
  DMA_ITConfig(DMA1_Channel5, DMA_IT_HT, ENABLE);			// interrupt on Half Transfer (HT)
  DMA_ITConfig(DMA1_Channel5, DMA_IT_TC, ENABLE);			// interrupt on Transfer Complete (TC)

  // Enable DMA1 Channel5 transfer
  DMA_Cmd(DMA1_Channel5, ENABLE);
}

void dma_mfmWrite_init(void)
{
	DMA_InitTypeDef DMA_InitStructure;
	
	// DMA1 channel6 configuration -- TIM3_CH1 (capture)
  DMA_DeInit(DMA1_Channel6);
	
  DMA_InitStructure.DMA_PeripheralBaseAddr	= (uint32_t) &(TIM3->DMAR);					// from this peripheral address
  DMA_InitStructure.DMA_MemoryBaseAddr			= (uint32_t) mfmWriteStreamBuffer;	// to this buffer located in memory
  DMA_InitStructure.DMA_DIR									= DMA_DIR_PeripheralSRC;						// dir: from periph to mem
  DMA_InitStructure.DMA_BufferSize					= 16;																// 16 datas to transfer
  DMA_InitStructure.DMA_PeripheralInc				= DMA_PeripheralInc_Disable;				// PINC = 0 -- don't icrement, always read from DMAR register
  DMA_InitStructure.DMA_MemoryInc						= DMA_MemoryInc_Enable;							// MINC = 1 -- increment in memory -- go though buffer
  DMA_InitStructure.DMA_PeripheralDataSize	= DMA_PeripheralDataSize_HalfWord;	// each data item: 16 bits 
  DMA_InitStructure.DMA_MemoryDataSize			= DMA_MemoryDataSize_HalfWord;			// each data item: 16 bits 
  DMA_InitStructure.DMA_Mode								= DMA_Mode_Circular;								// circular mode
  DMA_InitStructure.DMA_Priority						= DMA_Priority_Medium;
  DMA_InitStructure.DMA_M2M									= DMA_M2M_Disable;									// M2M disabled, because we move to peripheral
  DMA_Init(DMA1_Channel6, &DMA_InitStructure);

  // Enable DMA1 Channel6 Transfer Complete interrupt
  DMA_ITConfig(DMA1_Channel6, DMA_IT_HT, ENABLE);			// interrupt on Half Transfer (HT)
  DMA_ITConfig(DMA1_Channel6, DMA_IT_TC, ENABLE);			// interrupt on Transfer Complete (TC)

  // Enable DMA1 Channel6 transfer
  DMA_Cmd(DMA1_Channel6, ENABLE);
}

void dma_spi_init(void)
{
	DMA_InitTypeDef DMA_InitStructure;
	
	// DMA1 channel3 configuration -- SPI1 TX
  DMA_DeInit(DMA1_Channel3);
	
  DMA_InitStructure.DMA_MemoryBaseAddr			= (uint32_t) 0;											// from this buffer located in memory (now only fake)
  DMA_InitStructure.DMA_PeripheralBaseAddr	= (uint32_t) &(SPI1->DR);						// to this peripheral address
  DMA_InitStructure.DMA_DIR									= DMA_DIR_PeripheralDST;						// dir: from mem to periph
  DMA_InitStructure.DMA_BufferSize					= 0;																// fake buffer size
  DMA_InitStructure.DMA_PeripheralInc				= DMA_PeripheralInc_Disable;				// PINC = 0 -- don't icrement, always write to SPI1->DR register
  DMA_InitStructure.DMA_MemoryInc						= DMA_MemoryInc_Enable;							// MINC = 1 -- increment in memory -- go though buffer
  DMA_InitStructure.DMA_PeripheralDataSize	= DMA_PeripheralDataSize_HalfWord;	// each data item: 16 bits 
  DMA_InitStructure.DMA_MemoryDataSize			= DMA_MemoryDataSize_HalfWord;			// each data item: 16 bits 
  DMA_InitStructure.DMA_Mode								= DMA_Mode_Normal;									// normal mode
  DMA_InitStructure.DMA_Priority						= DMA_Priority_High;
  DMA_InitStructure.DMA_M2M									= DMA_M2M_Disable;									// M2M disabled, because we move to peripheral
  DMA_Init(DMA1_Channel3, &DMA_InitStructure);

  // Enable interrupts
//  DMA_ITConfig(DMA1_Channel3, DMA_IT_HT, ENABLE);			// interrupt on Half Transfer (HT)
  DMA_ITConfig(DMA1_Channel3, DMA_IT_TC, ENABLE);			// interrupt on Transfer Complete (TC)

  // Enable DMA1 Channel3 transfer
//  DMA_Cmd(DMA1_Channel3, ENABLE);
	
	//----------------
	// DMA1 channel2 configuration -- SPI1 RX
  DMA_DeInit(DMA1_Channel2);
	
  DMA_InitStructure.DMA_PeripheralBaseAddr	= (uint32_t) &(SPI1->DR);						// from this peripheral address
  DMA_InitStructure.DMA_MemoryBaseAddr			= (uint32_t) 0;											// to this buffer located in memory (now only fake)
  DMA_InitStructure.DMA_DIR									= DMA_DIR_PeripheralSRC;						// dir: from periph to mem
  DMA_InitStructure.DMA_BufferSize					= 0;																// fake buffer size
  DMA_InitStructure.DMA_PeripheralInc				= DMA_PeripheralInc_Disable;				// PINC = 0 -- don't icrement, always write to SPI1->DR register
  DMA_InitStructure.DMA_MemoryInc						= DMA_MemoryInc_Enable;							// MINC = 1 -- increment in memory -- go though buffer
  DMA_InitStructure.DMA_PeripheralDataSize	= DMA_PeripheralDataSize_HalfWord;	// each data item: 16 bits 
  DMA_InitStructure.DMA_MemoryDataSize			= DMA_MemoryDataSize_HalfWord;			// each data item: 16 bits 
  DMA_InitStructure.DMA_Mode								= DMA_Mode_Normal;									// normal mode
  DMA_InitStructure.DMA_Priority						= DMA_Priority_VeryHigh;
  DMA_InitStructure.DMA_M2M									= DMA_M2M_Disable;									// M2M disabled, because we move from peripheral
  DMA_Init(DMA1_Channel2, &DMA_InitStructure);

  // Enable DMA1 Channel2 Transfer Complete interrupt
//  DMA_ITConfig(DMA1_Channel2, DMA_IT_HT, ENABLE);			// interrupt on Half Transfer (HT)
  DMA_ITConfig(DMA1_Channel2, DMA_IT_TC, ENABLE);			// interrupt on Transfer Complete (TC)

  // Enable DMA1 Channel2 transfer
//  DMA_Cmd(DMA1_Channel2, ENABLE);
}
