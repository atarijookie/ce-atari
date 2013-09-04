#include "stm32f10x.h"                       // STM32F103 definitions
#include "stm32f10x_spi.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_dma.h"
#include "stm32f10x_exti.h"
#include "misc.h"

#include "defs.h"
#include "timers.h"

void init_hw_sw(void);

void processHostCommand(BYTE val);

void spi_init(void);
void dma_spi_init(void);

WORD spi_TxRx(WORD out);
void spiDma_txRx(WORD txCount, BYTE *txBfr, WORD rxCount, BYTE *rxBfr);

#define INBUFFER_SIZE					15000
BYTE inBuffer[INBUFFER_SIZE];
WORD inIndexGet;

TWriteBuffer wrBuffer[2];							// two buffers for written sectors
TWriteBuffer *wrNow;

// cycle measure: t1 = TIM3->CNT;	t2 = TIM3->CNT;	dt = t2 - t1; -- subtrack 0x12 because that's how much measuring takes
WORD t1, t2, dt; 

// digital osciloscope time measure on GPIO B0:
// 			GPIOB->BSRR = 1;	// 1
//			GPIOB->BRR = 1;		// 0			

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
#define ATN_SECTOR_WRITTEN      0x03               	// sent: 3, side (highest bit) + track #, current sector #
#define ATN_SEND_TRACK          0x04            		// send the whole track


// commands sent from host to device
#define CMD_WRITE_PROTECT_OFF			0x10
#define CMD_WRITE_PROTECT_ON			0x20
#define CMD_TRACK_STREAM_END			0xF000							// this is the mark in the track stream that we shouldn't go any further in the stream
#define CMD_TRACK_STREAM_END_BYTE	0xF0								// this is the mark in the track stream that we shouldn't go any further in the stream

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
//#define		inBuffer_markAndmove()			{ inBuffer[inIndexGet] = CMD_MARK_READ;		inIndexGet++;		if(inIndexGet >= INBUFFER_SIZE) { inIndexGet = 0; };	}
#define		inBuffer_justMove()					{ 																				inIndexGet++;		if(inIndexGet >= INBUFFER_SIZE) { inIndexGet = 0; };	}
//--------------
WORD version[2] = {0xa013, 0x0718};				// this means: hAns, 2013-09-04

volatile BYTE sendFwVersion, sendTrackRequest;
WORD atnSendFwVersion[5], atnSendTrackRequest[4];
BYTE cmdBuffer[12];
WORD fakeBuffer;

int main (void) 
{
	BYTE indexCount = 0;
	BYTE spiDmaIsIdle = TRUE;

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

	// init floppy signals
	GPIOB->BSRR = (WR_PROTECT | DISK_CHANGE);			// not write protected
	GPIOB->BRR = TRACK0 | DISK_CHANGE;						// TRACK 0 signal to L, DISK_CHANGE to LOW		
	GPIOB->BRR = ATN;															// ATTENTION bit low - nothing to read
	
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

				
				sendTrackRequest	= FALSE;
			}
		}
		
		if(DMA1_Channel3->CNDTR != 0) {												// something to send over SPI?
			GPIOB->BSRR = ATN;																	// ATTENTION bit high - got something to read
		} else {
			GPIOB->BRR = ATN;																		// ATTENTION bit low  - nothing to read
		}				 
		
		//-------------------------------------------------
		inputs = GPIOB->IDR;										// read floppy inputs

		
		//------------
		// the following part is here to send FW version each second
		if((TIM2->SR & 0x0001) != 0) {		// overflow of TIM1 occured?
			TIM2->SR = 0xfffe;							// clear UIF flag
	
			indexCount++;
			
			if(indexCount == 5) {
				indexCount = 0;
				sendFwVersion = TRUE;
			}
		}
		
// check for STEP pulse - should we go to a different track?
//		ints = EXTI->PR;										// Pending register (EXTI_PR)
		
//		if(ints & STEP) {										// if falling edge of STEP signal was found
//			EXTI->PR = STEP;									// clear that int
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
}

void init_hw_sw(void)
{
	RCC->AHBENR		|= (1 <<  0);																						// enable DMA1
	RCC->APB1ENR	|= (1 << 2) | (1 <<  1) | (1 <<  0);										// enable TIM4, TIM3, TIM2
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
	
	//----------
	AFIO->MAPR |= 0x02000000;									// SWJ_CFG[2:0] (Bits 26:24) -- 010: JTAG-DP Disabled and SW-DP Enabled
	AFIO->MAPR |= 0x00000300;									// TIM2_REMAP -- Full remap (CH1/ETR/PA15, CH2/PB3, CH3/PB10, CH4/PB11)
	AFIO->MAPR |= 0x00000800;									// TIM3_REMAP -- Partial remap (CH1/PB4, CH2/PB5, CH3/PB0, CH4/PB1)
	//----------
	
	timerSetup_index();
	
	//--------------
	// DMA + SPI initialization
	
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
	timerSetup_stepLimiter();						// this 2 kHz timer should be used to limit step rate
	
	// init circular buffer for data incomming via SPI
	inIndexGet = 0;
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

void processHostCommand(BYTE val)
{
	switch(val) {
		case CMD_WRITE_PROTECT_OFF:		GPIOB->BSRR	= WR_PROTECT;		break;			// WR PROTECT to 1
		case CMD_WRITE_PROTECT_ON:		GPIOB->BRR	= WR_PROTECT;		break;			// WR PROTECT to 0
	}
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
