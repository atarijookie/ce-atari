#include "stm32f10x.h"                       // STM32F103 definitions
#include "stm32f10x_spi.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_dma.h"
#include "stm32f10x_exti.h"
#include "misc.h"

#include "defs.h"
#include "timers.h"
#include "bridge.h"

void init_hw_sw(void);

void processHostCommands(void);

void spi_init(void);
void dma_spi_init(void);

WORD spi_TxRx(WORD out);
void spiDma_txRx(WORD txCount, BYTE *txBfr, WORD rxCount, BYTE *rxBfr);
void spiDma_waitForFinish(void);
BYTE spiDma_hasFinished(void);
void spiDma_clearFlags(void);

void getCmdLengthFromCmdBytes(void);
void onGetCommand(void);
void onDataRead(void);
void onDataWrite(void);
void onReadStatus(void);

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
#define ATN_FW_VERSION						0x01								// followed by string with FW version (length: 4 WORDs - cmd, v[0], v[1], 0)
#define ATN_ACSI_COMMAND					0x02
#define ATN_READ_MORE_DATA				0x03
#define ATN_WRITE_MORE_DATA				0x04
#define ATN_GET_STATUS						0x05

// commands sent from host to device
#define CMD_ACSI_CONFIG						0x10
#define CMD_DATA_WRITE						0x20
#define CMD_DATA_READ							0x30
#define CMD_SEND_STATUS						0x40

// these states define if the device should get command or transfer data
#define STATE_GET_COMMAND		0
#define STATE_DATA_READ			1
#define STATE_DATA_WRITE		2
#define STATE_READ_STATUS		3
BYTE state;
WORD dataCnt;
BYTE statusByte;

WORD version[2] = {0xa013, 0x0904};				// this means: hAns, 2013-09-04

volatile BYTE sendFwVersion, sendACSIcommand;
WORD atnSendFwVersion[5], atnSendACSIcommand[10];

WORD atnMoreData[4];
WORD dataBuffer[512 / 2];

WORD atnGetStatus[3];

TWriteBuffer wrBuf1, wrBuf2;
TWriteBuffer *wrBufNow;

#define CMD_BUFFER_LENGTH						28
BYTE cmdBuffer[CMD_BUFFER_LENGTH];

BYTE cmd[14];										// received command bytes
BYTE cmdLen;										// length of received command
BYTE brStat;										// status from bridge

BYTE enabledIDs[8];							// when 1, Hanz will react on that ACSI ID #

BYTE spiDmaIsIdle = TRUE;

int main (void) 
{
	BYTE i;
	
	state = STATE_GET_COMMAND;
	
	sendFwVersion		= FALSE;
	sendACSIcommand	= FALSE;
	
	atnSendFwVersion[0] = 0;											// just-in-case padding
	atnSendFwVersion[1] = ATN_FW_VERSION;					// attention code
	atnSendFwVersion[2] = version[0];
	atnSendFwVersion[3] = version[1];
	atnSendFwVersion[4] = 0;											// terminating zero
	
	atnSendACSIcommand[0] = 0;										// just-in-case padding
	atnSendACSIcommand[1] = ATN_ACSI_COMMAND;			// attention code
	atnSendACSIcommand[9] = 0;										// terminating zero
	
	atnMoreData[0] = 0;
	atnMoreData[1] = ATN_READ_MORE_DATA;					// mark that we want to read more data
	atnMoreData[3] = 0;
	
	wrBuf1.buffer[0]	= 0;
	wrBuf1.buffer[1]	= ATN_WRITE_MORE_DATA;
	wrBuf1.next				= (void *) &wrBuf2;

	wrBuf2.buffer[0]	= 0;
	wrBuf2.buffer[1]	= ATN_WRITE_MORE_DATA;
	wrBuf2.next				= (void *) &wrBuf1;

	wrBufNow = &wrBuf1;

	atnGetStatus[0] = 0;
	atnGetStatus[1] = ATN_GET_STATUS;
	atnGetStatus[2] = 0;

	init_hw_sw();																	// init GPIO pins, timers, DMA, global variables

	// init ACSI signals
	ACSI_DATADIR_WRITE();													// data as inputs
	GPIOA->BRR = aDMA | aPIO | aRNW | ATN;				// ACSI controll signals LOW, ATTENTION bit LOW - nothing to read

	EXTI->PR = aCMD | aCS | aACK;									// clear these ints 
	//-------------
	// by default set ID 0 as enabled and other IDs as disabled 
	enabledIDs[0] = 1;
	
	for(i=1; i<8; i++) {
		enabledIDs[i] = 0;
	}
	//-------------

	while(1) {
		// get the command from ACSI and send it to host
		if(state == STATE_GET_COMMAND && PIO_gotFirstCmdByte()) {					// if 1st CMD byte was received
			onGetCommand();
		}
		
		// transfer the data - read (to ST)
		if(state == STATE_DATA_READ) {
			onDataRead();
		}

		// transfer the data - write (from ST)
		if(state == STATE_DATA_WRITE) {
			onDataWrite();
		}
		
		// this happens after WRITE - wait for status byte, send it to ST (read)
		if(state == STATE_READ_STATUS) {
			onReadStatus();
		}

		// sending and receiving data over SPI using DMA
		if(spiDma_hasFinished()) {															// SPI DMA: nothing to Tx and nothing to Rx?
			spiDma_clearFlags();
			processHostCommands();																// and process all the received commands
		}

		if(spiDmaIsIdle == TRUE) {															// SPI DMA: nothing to Tx and nothing to Rx?
			if(sendFwVersion) {																		// should send FW version? this is a window for receiving commands
				spiDma_txRx(5, (BYTE *) &atnSendFwVersion[0],			 6, (BYTE *) &cmdBuffer[0]);
				
				sendFwVersion	= FALSE;
			} else if(sendACSIcommand) {
				spiDma_txRx(10, (BYTE *) &atnSendACSIcommand[0],	(CMD_BUFFER_LENGTH / 2),	(BYTE *) &cmdBuffer[0]);
				
				sendACSIcommand	= FALSE;
			}
		}
		
		if(DMA1_Channel3->CNDTR != 0) {												// something to send over SPI?
			GPIOA->BSRR = ATN;																	// ATTENTION bit high - got something to read
		} else {
			GPIOA->BRR = ATN;																		// ATTENTION bit low  - nothing to read
		}
		
		//-------------------------------------------------
		// the following part is here to send FW version each second
		if((TIM2->SR & 0x0001) != 0) {		// overflow of TIM1 occured?
			TIM2->SR = 0xfffe;							// clear UIF flag
			sendFwVersion = TRUE;
		}
	}
}

void onGetCommand(void)
{
	BYTE id, i;
				
	cmd[0] = PIO_writeFirst();				// get byte from ST (waiting for the 1st byte)
	id = (cmd[0] >> 5) & 0x07;				// get only device ID

	if(enabledIDs[id]) {							// if this ID is enabled
		cmdLen = 6;											// maximum 6 bytes at start, but this might change in getCmdLengthFromCmdBytes()
				
		for(i=1; i<cmdLen; i++) {				// receive the next command bytes
			cmd[i] = PIO_write();					// drop down IRQ, get byte

			if(brStat != E_OK) {					// if something was wrong
				break;						  
			}

			if(i == 1) {										// if we got also the 2nd byte
				getCmdLengthFromCmdBytes();		// we set up the length of command, etc.
			}   	  	  
		}
				
		//-----
		if(brStat == E_OK) {							// if all was well, let's send this to host
			for(i=0; i<7; i++) {						
				atnSendACSIcommand[2 + i] = (((WORD)cmd[i*2 + 0]) << 8) | cmd[i*2 + 1];
			}
					
			sendACSIcommand	= TRUE;
		}
	}
}

void onDataRead(void)
{
	WORD seqNo = 0;
	WORD subCount, recvCount, sendCount;
	WORD index, data;
	BYTE value;

	state = STATE_GET_COMMAND;														// this will be the next state once this function finishes
	
	ACSI_DATADIR_READ();																	// data direction for reading
	
	while(dataCnt > 0) {			// something to read?
		// request maximum 512 bytes from host 
		subCount = (dataCnt > 512) ? 512 : dataCnt;
		dataCnt -= subCount;
				
		atnMoreData[2] = seqNo;															// set the sequence # to Attention
		seqNo++;
		
		recvCount	= subCount / 2;														// WORDs to receive: convert # of BYTEs to # of WORDs
		recvCount	+= (subCount & 1);												// if subCount is odd number, then we need to transfer 1 WORD more
		
		sendCount	= (recvCount > 4) ? 4 : recvCount;				// WORDs to send: maximum 4 WORDs, but could be less

		spiDma_waitForFinish();
		spiDma_clearFlags();
		
		spiDma_txRx(sendCount, (BYTE *) &atnMoreData[0], recvCount, (BYTE *) &dataBuffer[0]);		// set up the SPI DMA transfer

		index = 0;
		
		while(recvCount > 0) {															// something to receive?
			// when at least 1 WORD was received
			if((DMA1_Channel2->CNDTR == 0) || (DMA1_Channel2->CNDTR < recvCount)) {
				recvCount--;				
				data = dataBuffer[index];												// get data
				index++;

				// for upper byte
				value = data >> 8;															// get upper byte
				subCount--;																			// decrement count
				
				DMA_read(value);																// send data to Atari
				
				if(brStat == E_TimeOut) {												// if timeout occured
					GPIOA->BRR = ATN;															// ATTENTION bit low  - nothing to read
					ACSI_DATADIR_WRITE();													// data direction for writing, and quit
					return;
				}

				if(subCount == 0) {															// no more data to transfer (in case of odd data count)
					break;
				}
				
				// for lower byte
				value = data & 0xff;														// get lower byte
				subCount--;																			// decrement count
				
				DMA_read(value);																// send data to Atari

				if(brStat == E_TimeOut) {												// if timeout occured
					GPIOA->BRR = ATN;															// ATTENTION bit low  - nothing to read
					ACSI_DATADIR_WRITE();													// data direction for writing, and quit
					return;
				}
			}
			
			if(DMA1_Channel3->CNDTR == 0) {										// nothing to send? clear ATN bit
				GPIOA->BRR = ATN;																// ATTENTION bit low  - nothing to read
			}
		}

		GPIOA->BRR = ATN;																		// ATTENTION bit low  - nothing to read
	}	
	
	PIO_read(statusByte);																	// send the status to Atari
}

void onDataWrite(void)
{
	WORD seqNo = 0;
	WORD subCount, recvCount;
	WORD index, data, i, value;

	ACSI_DATADIR_WRITE();																	// data direction for reading
	
	while(dataCnt > 0) {			// something to write?
		// request maximum 512 bytes from host 
		subCount = (dataCnt > 512) ? 512 : dataCnt;
		dataCnt -= subCount;
				
		wrBufNow->buffer[2] = seqNo;												// set the sequence # to Attention
		seqNo++;
		
		recvCount	= subCount / 2;														// WORDs to receive: convert # of BYTEs to # of WORDs
		recvCount	+= (subCount & 1);												// if subCount is odd number, then we need to transfer 1 WORD more
		
		index = 3;
		
		for(i=0; i<recvCount; i++) {											// write this many WORDs
			if(DMA1_Channel3->CNDTR == 0) {										// nothing to send? clear ATN bit
				GPIOA->BRR = ATN;																// ATTENTION bit low  - nothing to read
			}
			
			value = DMA_write();														// get data from Atari
			value = value << 8;															// store as upper byte
				
			if(brStat == E_TimeOut) {												// if timeout occured
				state = STATE_GET_COMMAND;										// transfer failed, don't send status, just get next command
				GPIOA->BRR = ATN;															// ATTENTION bit low  - nothing to read
				return;
			}

			subCount--;
			if(subCount == 0) {															// in case of odd data count
				wrBufNow->buffer[index] = value;							// store data
				index++;
				
				break;
			}

			data = DMA_write();															// get data from Atari
			value = value | data;														// store as lower byte
				
			if(brStat == E_TimeOut) {												// if timeout occured
				state = STATE_GET_COMMAND;										// transfer failed, don't send status, just get next command
				GPIOA->BRR = ATN;															// ATTENTION bit low  - nothing to read
				return;
			}

			subCount--;
							
			wrBufNow->buffer[index] = value;								// store data
			index++;
		}
		
		wrBufNow->buffer[index] = 0;											// terminating zero
		wrBufNow->count = index + 1;											// store count

		spiDma_waitForFinish();
		spiDma_clearFlags();
		
		// set up the SPI DMA transfer
		spiDma_txRx(wrBufNow->count, (BYTE *) &wrBufNow->buffer[0], 0, (BYTE *) &dataBuffer[0]);		
		
		wrBufNow = wrBufNow->next;												// use next write buffer
	}

	state = STATE_READ_STATUS;													// continue with sending the status
	GPIOA->BRR = ATN;																		// ATTENTION bit low  - nothing to read

}

void onReadStatus(void)
{
	BYTE i, newStatus;
	
	newStatus = 0xff;																	// no status received
	
	spiDma_waitForFinish();
	spiDma_clearFlags();

	spiDma_txRx(3, (BYTE *) &atnGetStatus[0], 5, (BYTE *) &cmdBuffer[0]);

	spiDma_waitForFinish();
	spiDma_clearFlags();

	for(i=0; i<10; i++) {															// go through the received buffer
		if(cmdBuffer[i] == CMD_SEND_STATUS) {
			newStatus = cmdBuffer[i+1];
			break;
		}
	}

	PIO_read(newStatus);																// send the status to Atari
	state = STATE_GET_COMMAND;													// get the next command
}

void getCmdLengthFromCmdBytes(void)
{	
	// now it's time to set up the receiver buffer and length
	if((cmd[0] & 0x1f)==0x1f)	{		  // if the command is '0x1f'
		switch((cmd[1] & 0xe0)>>5)	 		// get the length of the command
		{
			case  0: cmdLen =  7; break;
			case  1: cmdLen = 11; break;
			case  2: cmdLen = 11; break;
			case  5: cmdLen = 13; break;
			default: cmdLen =  7; break;
		}
	} else {		 	 	   		 						// if it isn't a ICD command
		cmdLen   = 6;	  									// then length is 6 bytes 
	}
}

void spiDma_waitForFinish(void)
{
	if(spiDmaIsIdle) {							// if DMA is idle, just quit
		return;
	}
	
	while(1) {											// otherwise wait until it will become idle
		if(spiDma_hasFinished()) {
			return;
		}
	}
}

BYTE spiDma_hasFinished(void)
{
	if(spiDmaIsIdle) {							// if DMA is idle, then DMA has finished
		return TRUE;
	}
	
	if((DMA1->ISR & (DMA1_IT_TC3 | DMA1_IT_TC2)) == (DMA1_IT_TC3 | DMA1_IT_TC2)) {	// SPI DMA: nothing to Tx and nothing to Rx?
		GPIOA->BRR = ATN;																			// ATTENTION bit low - nothing to read
		return TRUE;
	}
	
	return FALSE;
}

void spiDma_clearFlags(void)
{
	DMA1->IFCR		= DMA1_IT_TC3 | DMA1_IT_TC2;						// clear HTIF flags
	spiDmaIsIdle	= TRUE;																	// mark that the SPI DMA is ready to do something
	GPIOA->BRR		= ATN;																	// ATTENTION bit low - nothing to read
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
	
	// now set the ATN pin accordingly
	if(txCount != 0) {																	// something to send over SPI?
		GPIOA->BSRR = ATN;																// ATTENTION bit high - got something to read
	} else {
		GPIOA->BRR = ATN;																	// ATTENTION bit low  - nothing to read
	}
	
	spiDmaIsIdle = FALSE;																// SPI DMA is busy
}

void init_hw_sw(void)
{
	RCC->AHBENR		|= (1 <<  0);																						// enable DMA1
	RCC->APB1ENR	|= (1 << 2) | (1 <<  1) | (1 <<  0);										// enable TIM4, TIM3, TIM2
  RCC->APB2ENR	|= (1 << 12) | (1 << 11) | (1 << 3) | (1 << 2);     		// Enable SPI1, TIM1, GPIOA and GPIOB clock

	// SPI -- enable atlernate function for PA4, PA5, PA6, PA7
	GPIOA->CRL &= ~(0xffff0000);						// remove bits from GPIOA
	GPIOA->CRL |=   0xbbbb0000;							// set GPIOA as --- CNF1:0 -- 10 (push-pull), MODE1:0 -- 11, PxODR -- don't care

	// ACSI input signals as floating inputs
	GPIOB->CRH &= ~(0x0000ffff);						// clear 4 lowest bits
	GPIOB->CRH |=   0x00004444;							// set as 4 inputs (ACK, CS, CMD, RESET)

	// ACSI control signals as output, +ATN
	GPIOA->CRL &= ~(0x0000ffff);						// remove bits from GPIOA
	GPIOA->CRL |=   0x00003333;							// 4 ouputs (ATN, DMA, PIO, RNW) and 4 inputs (ACK, CS, CMD, RESET)

	RCC->APB2ENR |= (1 << 0);								// enable AFIO
	
	AFIO->EXTICR[2] = 0x1110;								// source input: GPIOB for EXTI 9, 10, 11
	EXTI->IMR			= aCMD | aCS | aACK;			// 1 means: Interrupt from these lines is not masked
	EXTI->EMR			= aCMD | aCS | aACK;			// 1 means: Event     form these lines is not masked
	EXTI->FTSR 		= aCMD | aCS | aACK;			// Falling trigger selection register
	
	//----------
	AFIO->MAPR |= 0x02000000;								// SWJ_CFG[2:0] (Bits 26:24) -- 010: JTAG-DP Disabled and SW-DP Enabled
	
	timerSetup_sendFw();										// set TIM2 to 1 second to send FW version each second
	timerSetup_cmdTimeout();								// set TIM4 to 1 second as a timeout for command processing
	
	//--------------
	// DMA + SPI initialization
	
	spi_init();																		// init SPI interface
	dma_spi_init();
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

void processHostCommands(void)
{
	BYTE i, j;
	WORD ids;
	
	for(i=0; i<(CMD_BUFFER_LENGTH - 1); i++) {
		switch(cmdBuffer[i]) {
			// process configuration 
			case CMD_ACSI_CONFIG:
					ids = cmdBuffer[i+1];					// get enabled IDs
			
					for(j=0; j<8; j++) {					// 
						if(ids & (1<<j)) {
							enabledIDs[j] = TRUE;
						} else {
							enabledIDs[j] = FALSE;
						}
					}
					
					cmdBuffer[i]		= 0;							// clear this command
					cmdBuffer[i+1]	= 0;
					
					i++;
				break;
				
			case CMD_DATA_WRITE:
					dataCnt				= (cmdBuffer[i+1] << 8) | cmdBuffer[i+2];			// store the count of bytes we should WRITE
					statusByte		= cmdBuffer[i+3];
					
					for(j=0; j<4; j++) {					// clear this command 
						cmdBuffer[i + j] = 0;				
					}
					
					state = STATE_DATA_WRITE;			// go into DATA_WRITE state
					i += 3;
				break;
				
			case CMD_DATA_READ:		
					dataCnt				= (cmdBuffer[i+1] << 8) | cmdBuffer[i+2];			// store the count of bytes we should READ
					statusByte		= cmdBuffer[i+3];

					for(j=0; j<4; j++) {					// clear this command 
						cmdBuffer[i + j] = 0;				
					}

					state = STATE_DATA_READ;			// go into DATA_READ state
					i += 3;
				break;
		}
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
  DMA_ITConfig(DMA1_Channel3, DMA_IT_TC, ENABLE);			// interrupt on Transfer Complete (TC)

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
  DMA_ITConfig(DMA1_Channel2, DMA_IT_TC, ENABLE);			// interrupt on Transfer Complete (TC)
}
