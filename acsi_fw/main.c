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
void onButtonPress(void);

void processHostCommands(void);

void spi_init(void);
void dma_spi_init(void);

WORD spi_TxRx(WORD out);
void spiDma_txRx(WORD txCount, BYTE *txBfr, WORD rxCount, BYTE *rxBfr);
void spiDma_waitForFinish(void);

void getCmdLengthFromCmdBytes(void);
void onGetCommand(void);
void onDataRead(void);
void onDataWrite(void);
void onReadStatus(void);

// cycle measure: t1 = TIM3->CNT;	t2 = TIM3->CNT;	dt = t2 - t1; -- subtract 0x12 because that's how much measuring takes
WORD t1, t2, dt; 

// digital osciloscope time measure on GPIO B0:
// 			GPIOB->BSRR = 1;	// 1
//			GPIOB->BRR = 1;		// 0			

// commands sent from device to host
#define ATN_FW_VERSION						0x01								// followed by string with FW version (length: 4 WORDs - cmd, v[0], v[1], 0)
#define ATN_ACSI_COMMAND					0x02
#define ATN_READ_MORE_DATA				0x03
#define ATN_WRITE_MORE_DATA				0x04
#define ATN_GET_STATUS						0x05

// commands sent from host to device
#define CMD_ACSI_CONFIG						0x10
#define CMD_DATA_WRITE						0x20
#define CMD_DATA_READ						0x30
#define CMD_SEND_STATUS						0x40
#define CMD_DATA_MARKER						0xda

// these states define if the device should get command or transfer data
#define STATE_GET_COMMAND					0
#define STATE_DATA_READ						1
#define STATE_DATA_WRITE					2
#define STATE_READ_STATUS					3


#define CMD_BUFFER_LENGTH					16

///////////////////////////
// The following definitions are definitions of how many WORDs are TXed and RXed for each different ATN.
// Note that ATN_VARIABLE_LEN shouldn't be used, and is replaced by some value.

#define ATN_VARIABLE_LEN						0xffff

#define ATN_SENDFWVERSION_LEN_TX		8
#define ATN_SENDFWVERSION_LEN_RX		8

#define ATN_SENDACSICOMMAND_LEN_TX	12
#define ATN_SENDACSICOMMAND_LEN_RX	CMD_BUFFER_LENGTH

#define ATN_READMOREDATA_LEN_TX			6	
#define ATN_READMOREDATA_LEN_RX			ATN_VARIABLE_LEN

#define ATN_WRITEMOREDATA_LEN_TX		ATN_VARIABLE_LEN
#define ATN_WRITEMOREDATA_LEN_RX		1

#define ATN_GETSTATUS_LEN_TX				5
#define ATN_GETSTATUS_LEN_RX				8

///////////////////////////

BYTE state;
DWORD dataCnt;
BYTE statusByte;

WORD version[2] = {0xa013, 0x0904};				// this means: hAns, 2013-09-04

volatile BYTE sendFwVersion, sendACSIcommand;
WORD atnSendFwVersion[ATN_SENDFWVERSION_LEN_TX];
WORD atnSendACSIcommand[ATN_SENDACSICOMMAND_LEN_TX];

WORD atnMoreData[ATN_READMOREDATA_LEN_TX];
WORD dataBuffer[550 / 2];				// sector buffer with some (38 bytes) reserve at the end in case of overflow

WORD atnGetStatus[ATN_GETSTATUS_LEN_TX];

TWriteBuffer wrBuf1, wrBuf2;
TWriteBuffer *wrBufNow;

WORD cmdBuffer[CMD_BUFFER_LENGTH];

BYTE cmd[14];										// received command bytes
BYTE cmdLen;										// length of received command
BYTE brStat;										// status from bridge

BYTE enabledIDs[8];							// when 1, Hanz will react on that ACSI ID #

volatile BYTE spiDmaIsIdle;
volatile BYTE spiDmaTXidle, spiDmaRXidle;		// flags set when the SPI DMA TX or RX is idle

BYTE currentLed;

// TODO: code gets stuck if AcsiDataTrans sends ODD number of bytes 
// TODO: have to send bit more than Multiple of 16 to receive all data ( padDataToMul16 in host SW )
// TODO: check why the communication with host gets stuck when ST of off
// TODO: test and fix with megafile drive

// TODO: CEDD downloader - read po jedinom sektore, zapis na floppy

// note: DMA transfer to ODD address in ST looses first byte

BYTE shouldProcessCommands;

int main (void) 
{
	BYTE i;
	
	state = STATE_GET_COMMAND;
	
	sendFwVersion		= FALSE;
	sendACSIcommand	= FALSE;
	
	atnSendFwVersion[0] = 0;											// just-in-case padding
	atnSendFwVersion[1] = ATN_FW_VERSION;					// attention code
	// WORDs 2 and 3 are reserved for TX LEN and RX LEN
	atnSendFwVersion[4] = version[0];
	atnSendFwVersion[5] = version[1];
	// WORD 6 is reserved for current LED (floppy image) number
	atnSendFwVersion[7] = 0;											// terminating zero
	
	atnSendACSIcommand[0] = 0;										// just-in-case padding
	atnSendACSIcommand[1] = ATN_ACSI_COMMAND;			// attention code
	// WORDs 2 and 3 are reserved for TX LEN and RX LEN
	atnSendACSIcommand[11] = 0;										// terminating zero
	
	atnMoreData[0] = 0;
	atnMoreData[1] = ATN_READ_MORE_DATA;					// mark that we want to read more data
	// WORDs 2 and 3 are reserved for TX LEN and RX LEN
	// WORD 4 is sequence number of the request for this command
	atnMoreData[5] = 0;
	
	wrBuf1.buffer[0]	= 0;
	wrBuf1.buffer[1]	= ATN_WRITE_MORE_DATA;
	// WORDs 2 and 3 are reserved for TX LEN and RX LEN
	// WORD 4 is sequence number of the request for this command
	wrBuf1.next				= (void *) &wrBuf2;

	wrBuf2.buffer[0]	= 0;
	wrBuf2.buffer[1]	= ATN_WRITE_MORE_DATA;
	// WORDs 2 and 3 are reserved for TX LEN and RX LEN
	// WORD 4 is sequence number of the request for this command
	wrBuf2.next			= (void *) &wrBuf1;

	wrBufNow = &wrBuf1;

	atnGetStatus[0] = 0;
	atnGetStatus[1] = ATN_GET_STATUS;
	// WORDs 2 and 3 are reserved for TX LEN and RX LEN
	atnGetStatus[4] = 0;

	spiDmaIsIdle = TRUE;
	spiDmaTXidle = TRUE;
	spiDmaRXidle = TRUE;
	
	currentLed = 0;

	shouldProcessCommands = FALSE;

	init_hw_sw();																	// init GPIO pins, timers, DMA, global variables

	// init ACSI signals
	ACSI_DATADIR_WRITE();													// data as inputs
	GPIOA->BRR = aDMA | aPIO | aRNW | ATN;				// ACSI controll signals LOW, ATTENTION bit LOW - nothing to read

	EXTI->PR = BUTTON | aCMD | aCS | aACK;				// clear these ints 
	//-------------
	// by default set ID 0 as enabled and other IDs as disabled 
	enabledIDs[0] = 1;
	
	for(i=1; i<8; i++) {
		enabledIDs[i] = 0;
	}
	
	//-------------
	// reset the XILINX, because it might be in a weird state on power on
	GPIOB->BSRR	= XILINX_RESET;														// HIGH
	GPIOB->BRR	= XILINX_RESET;														// LOW

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
		if(spiDmaIsIdle && shouldProcessCommands) {							// SPI DMA: nothing to Tx and nothing to Rx?
			processHostCommands();																// and process all the received commands
			
			shouldProcessCommands = FALSE;												// mark that we don't need to process commands until next time
		}

		// in command waiting state, nothing to do and should send FW version?
		if(state == STATE_GET_COMMAND && spiDmaIsIdle && sendFwVersion) {
			atnSendFwVersion[6] = ((WORD)currentLed) << 8;				// store the current LED status in the last WORD
			
			timeoutStart();																				// start timeout counter so we won't get stuck somewhere
			
			spiDma_txRx(	ATN_SENDFWVERSION_LEN_TX, (BYTE *) &atnSendFwVersion[0],	 
										ATN_SENDFWVERSION_LEN_RX, (BYTE *) &cmdBuffer[0]);
				
			sendFwVersion	= FALSE;
			shouldProcessCommands = TRUE;													// mark that we should process the commands on next SPI DMA idle time
		}

		// SPI is idle and we should send command to host? 
		if(spiDmaIsIdle && sendACSIcommand) {
			spiDma_txRx(	ATN_SENDACSICOMMAND_LEN_TX, (BYTE *) &atnSendACSIcommand[0], 
										ATN_SENDACSICOMMAND_LEN_RX, (BYTE *) &cmdBuffer[0]);
				
			sendACSIcommand	= FALSE;
			shouldProcessCommands = TRUE;													// mark that we should process the commands on next SPI DMA idle time
		}
		
		// if the button was pressed, handle it
		if(EXTI->PR & BUTTON) {
			onButtonPress();
		}
		
		//-------------------------------------------------
		// the following part is here to send FW version each second
		if((TIM2->SR & 0x0001) != 0) {		// overflow of TIM2 occured?
			TIM2->SR = 0xfffe;							// clear UIF flag
			sendFwVersion = TRUE;
		}
	}
}

void onButtonPress(void)
{
	static WORD prevCnt = 0;
	WORD cnt;

	EXTI->PR = BUTTON;									// clear pending EXTI
	
	cnt = TIM1->CNT;										// get the current time
	
	if((cnt - prevCnt) < 500) {					// the previous button press happened less than 250 ms before? ignore
		return;
	}	
	
	prevCnt = cnt;											// store current time
	
	currentLed++;												// change LED
	
	if(currentLed > 2) {								// overflow?
		currentLed = 0;
	}
	
	GPIOA->BSRR = LED1 | LED2 | LED3;		// all LED pins high (LEDs OFF)
	
	switch(currentLed) {								// turn on the correct LED
			case 0:	GPIOA->BRR = LED1; break;
			case 1:	GPIOA->BRR = LED2; break;
			case 2:	GPIOA->BRR = LED3; break;
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
				atnSendACSIcommand[4 + i] = (((WORD)cmd[i*2 + 0]) << 8) | cmd[i*2 + 1];
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
	BYTE value, dataMarkerFound;

	state = STATE_GET_COMMAND;														// this will be the next state once this function finishes
	
	ACSI_DATADIR_READ();																	// data direction for reading
	
	while(dataCnt > 0) {			// something to read?
		// request maximum 512 bytes from host 
		subCount = (dataCnt > 512) ? 512 : dataCnt;
		dataCnt -= subCount;
				
		atnMoreData[4] = seqNo;															// set the sequence # to Attention
		seqNo++;
		
		// note that recvCount is good as count for how much we should receive over SPI, but not how much we should send to ST
		recvCount	= subCount / 2;														// WORDs to receive: convert # of BYTEs to # of WORDs
		recvCount	+= (subCount & 1);												// if subCount is odd number, then we need to transfer 1 WORD more
		recvCount += 6;																			// receive few words more than just data - 2 WORDs start, 1 WORD CMD_DATA_MARKER
		
		sendCount	= (recvCount > ATN_READMOREDATA_LEN_TX) ? ATN_READMOREDATA_LEN_TX : recvCount;				// WORDs to send: maximum ATN_READMOREDATA_LEN_TX WORDs, but could be less

		spiDma_txRx(	sendCount, (BYTE *) &atnMoreData[0], 
									recvCount, (BYTE *) &dataBuffer[0]);		// set up the SPI DMA transfer

		index = 0;
		
		// first wait for CMD_DATA_MARKER to appear
		dataMarkerFound = FALSE;
		
		while(recvCount > 0) {															// something to receive?
			if(timeout()) {																		// if the data from host doesn't come within timeout, quit
//					spiDmaIsIdle = TRUE;
					ACSI_DATADIR_WRITE();													// data direction for writing, and quit
					return;
			}
			
			// when at least 1 WORD was received
			if((DMA1_Channel2->CNDTR == 0) || (DMA1_Channel2->CNDTR < recvCount)) {
				data = dataBuffer[index];												// get data

				recvCount--;				
				index++;

				if(data == CMD_DATA_MARKER) {										// found data marker?
					dataMarkerFound = TRUE;
					break;
				}
			}
		}
		
		if(dataMarkerFound == FALSE) {										// didn't find the data marker?
			PIO_read(0x02);																	// send status: CHECK CONDITION and quit
			return;
		}

		// calculate again how many WORDs we should send to ST
		recvCount	= subCount / 2;														// WORDs to receive: convert # of BYTEs to # of WORDs
		recvCount	+= (subCount & 1);												// if subCount is odd number, then we need to transfer 1 WORD more

		// now try to trasmit the data
		while(recvCount > 0) {															// something to receive?
			if(timeout()) {																		// if the data from host doesn't come within timeout, quit
//					spiDmaIsIdle = TRUE;
					ACSI_DATADIR_WRITE();													// data direction for writing, and quit
					return;
			}

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
					ACSI_DATADIR_WRITE();													// data direction for writing, and quit
					return;
				}
			}
		}
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
				
		wrBufNow->buffer[4] = seqNo;												// set the sequence # to Attention
		seqNo++;
		
		recvCount	= subCount / 2;														// WORDs to receive: convert # of BYTEs to # of WORDs
		recvCount	+= (subCount & 1);												// if subCount is odd number, then we need to transfer 1 WORD more
		
		index = 5;																// length of header before data
		
		for(i=0; i<recvCount; i++) {											// write this many WORDs
			value = DMA_write();														// get data from Atari
			value = value << 8;															// store as upper byte
				
			if(brStat == E_TimeOut) {												// if timeout occured
				state = STATE_GET_COMMAND;										// transfer failed, don't send status, just get next command
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
				return;
			}

			subCount--;
							
			wrBufNow->buffer[index] = value;								// store data
			index++;
		}
		
		wrBufNow->buffer[index] = 0;											// terminating zero
		wrBufNow->count = index + 1;											// store count, +1 because we have terminating zero

		// set up the SPI DMA transfer
		spiDma_txRx(wrBufNow->count,			(BYTE *) &wrBufNow->buffer[0], 
					ATN_WRITEMOREDATA_LEN_RX,	(BYTE *) &dataBuffer[0]);		
		
		wrBufNow = wrBufNow->next;												// use next write buffer
	}

	state = STATE_READ_STATUS;													// continue with sending the status
}

void onReadStatus(void)
{
	BYTE i, newStatus;
	
	newStatus = 0xff;																	// no status received
	
	spiDma_txRx(	ATN_GETSTATUS_LEN_TX, (BYTE *) &atnGetStatus[0], 
								ATN_GETSTATUS_LEN_RX, (BYTE *) &cmdBuffer[0]);

	spiDma_waitForFinish();

	for(i=0; i<8; i++) {															// go through the received buffer
		if(cmdBuffer[i] == CMD_SEND_STATUS) {
			newStatus = cmdBuffer[i+1] >> 8;
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
	while(spiDmaIsIdle != TRUE) {		// wait until it will become idle
		if(timeout()) {								// if timeout happened (and we got stuck here), quit
			spiDmaIsIdle = TRUE;
			break;
		}
	}
}

void spiDma_txRx(WORD txCount, BYTE *txBfr, WORD rxCount, BYTE *rxBfr)
{
	WORD *pTxBfr = (WORD *) txBfr;
	
	// store TX and RX count so the host will know how much he should transfer
	pTxBfr[2] = txCount;
	pTxBfr[3] = rxCount;
	
	spiDma_waitForFinish();															// make sure that the last transfer has finished

	// disable both TX and RX channels
	DMA1_Channel3->CCR		&= 0xfffffffe;								// disable DMA3 Channel transfer
	DMA1_Channel2->CCR		&= 0xfffffffe;								// disable DMA2 Channel transfer

	// set the software flags of SPI DMA being idle
	spiDmaTXidle = (txCount == 0) ? TRUE : FALSE;				// if nothing to send, then IDLE; if something to send, then FALSE
	spiDmaRXidle = (rxCount == 0) ? TRUE : FALSE;				// if nothing to receive, then IDLE; if something to receive, then FALSE
	spiDmaIsIdle = FALSE;																// SPI DMA is busy
	
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
	}
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
	GPIOB->CRH &= ~(0x000fffff);						// clear 5 lowest bits
	GPIOB->CRH |=   0x00044444;							// set as 5 inputs (BUTTON, ACK, CS, CMD, RESET)

	// ACSI control signals as output, +ATN
	GPIOA->CRL &= ~(0x0000ffff);						// remove bits from GPIOA
	GPIOA->CRL |=   0x00003333;							// 4 ouputs (ATN, DMA, PIO, RNW) and 4 inputs (ACK, CS, CMD, RESET)

	// LEDs
	GPIOA->CRH &= ~(0xf00ff000);						// remove bits from GPIOA
	GPIOA->CRH |=   0x30033000;							// 3 ouputs - LED1, LED2, LED3

	// XILINX reset as output
	GPIOB->CRH &= ~(0xf0000000);						// clear 
	GPIOB->CRH |=   0x30000000;							// set XILINX reset as output
	
	RCC->APB2ENR |= (1 << 0);								// enable AFIO
	
	AFIO->EXTICR[2] = 0x1110;												// source input: GPIOB for EXTI 9, 10, 11
	AFIO->EXTICR[3] = 0x0001;												// source input: GPIOB for EXTI 12 -- button
	EXTI->IMR			= BUTTON | aCMD | aCS | aACK;			// 1 means: Interrupt from these lines is not masked
	EXTI->EMR			= BUTTON | aCMD | aCS | aACK;			// 1 means: Event     form these lines is not masked
	EXTI->FTSR 		= BUTTON | aCMD | aCS | aACK;			// Falling trigger selection register

	GPIOA->BSRR = LED1 | LED2 | LED3;		// all LED pins high (LEDs OFF)
	//----------
	AFIO->MAPR |= 0x02000000;								// SWJ_CFG[2:0] (Bits 26:24) -- 010: JTAG-DP Disabled and SW-DP Enabled
	
	timerSetup_buttonTimer();								// TIM1 counts at 2 kHz as a button timer (to not allow many buttons presses one after another)
	timerSetup_sendFw();										// set TIM2 to 1 second to send FW version each second
	timerSetup_cmdTimeout();								// set TIM3 to 1 second as a timeout for command processing
	
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
	
	for(i=0; i<CMD_BUFFER_LENGTH; i++) {
		switch(cmdBuffer[i]) {
			// process configuration 
			case CMD_ACSI_CONFIG:
					ids = cmdBuffer[i+1] >> 8;		// get enabled IDs
			
					for(j=0; j<8; j++) {					// for each bit in ids set the flag in enabledIDs[]
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
					dataCnt				= cmdBuffer[i+1];					// store the count of bytes we should WRITE - highest and middle byte
					dataCnt				= dataCnt << 8;
					dataCnt				|= cmdBuffer[i+2] >> 8;		// lowest byte
			
					statusByte		= cmdBuffer[i+2] & 0xff;
					
					for(j=0; j<3; j++) {										// clear this command 
						cmdBuffer[i + j] = 0;				
					}
					
					state = STATE_DATA_WRITE;								// go into DATA_WRITE state
					i += 2;
				break;
				
			case CMD_DATA_READ:		
					dataCnt				= cmdBuffer[i+1];					// store the count of bytes we should READ - highest and middle byte
					dataCnt				= dataCnt << 8;
					dataCnt				|= cmdBuffer[i+2] >> 8;		// lowest byte

					statusByte		= cmdBuffer[i+2] & 0xff;

					for(j=0; j<3; j++) {					// clear this command 
						cmdBuffer[i + j] = 0;				
					}

					state = STATE_DATA_READ;			// go into DATA_READ state
					i += 2;
				break;
		}
	}
}

// the interrupt on DMA SPI TX finished should minimize the need for checking and reseting ATN pin
void DMA1_Channel3_IRQHandler(void)
{
	DMA_ClearITPendingBit(DMA1_IT_TC3);								// possibly DMA1_IT_GL3 | DMA1_IT_TC3

	GPIOA->BRR = ATN;																	// ATTENTION bit low  - nothing to read

	spiDmaTXidle = TRUE;															// SPI DMA TX now idle
	
	if(spiDmaRXidle == TRUE) {												// and if even the SPI DMA RX is idle, SPI is idle completely
		spiDmaIsIdle = TRUE;														// SPI DMA is busy
	}
}

// interrupt on Transfer Complete of SPI DMA RX channel
void DMA1_Channel2_IRQHandler(void)
{
	DMA_ClearITPendingBit(DMA1_IT_TC2);								// possibly DMA1_IT_GL2 | DMA1_IT_TC2

	spiDmaRXidle = TRUE;															// SPI DMA RX now idle
	
	if(spiDmaTXidle == TRUE) {												// and if even the SPI DMA TX is idle, SPI is idle completely
		spiDmaIsIdle = TRUE;														// SPI DMA is busy
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
	NVIC_InitTypeDef NVIC_InitStructure;
	
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
	
	//----------------
	// now enable interrupt on DMA1_Channel3
  NVIC_InitStructure.NVIC_IRQChannel										= DMA1_Channel3_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority	= 0x01;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority					= 0x01;
  NVIC_InitStructure.NVIC_IRQChannelCmd									= ENABLE;
  NVIC_Init(&NVIC_InitStructure);

	// and also interrupt on DMA1_Channel2
  NVIC_InitStructure.NVIC_IRQChannel										= DMA1_Channel2_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority	= 0x02;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority					= 0x02;
  NVIC_InitStructure.NVIC_IRQChannelCmd									= ENABLE;
  NVIC_Init(&NVIC_InitStructure);
}
