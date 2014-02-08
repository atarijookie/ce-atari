#ifndef DEFS_H_
#define DEFS_H_

#define BYTE	unsigned char
#define WORD	unsigned short
#define DWORD	unsigned int

#define TRUE	1
#define FALSE	0

typedef struct 
{
	WORD buffer[550 / 2];			// buffer for the written data - with some (38 bytes) reserve at the end in case of overflow
	WORD count;								// count of WORDs in buffer 
	
	void *next;								// pointer to the next available TAtnBuffer
} TWriteBuffer;

typedef struct 
{
	WORD buffer[550 / 2];			// buffer for the written data - with some (38 bytes) reserve at the end in case of overflow
	WORD count;								// count of WORDs in buffer - may include header, markers, data, terminating marker
	WORD dataBytesCount;			// count of BYTEs of just data in buffer
	
	void *next;								// pointer to the next available TAtnBuffer
} TReadBuffer;

/*
reserved:
---------
GPIOA_4  - SPI
GPIOA_5  - SPI
GPIOA_6  - SPI
GPIOA_7  - SPI

GPIOA_9  - USART1_TX -- for bootloader
GPIOA_10 - USART1_RX -- for bootloader

GPIOA_13 - SWD -- for debugging
GPIOA_14 - SWD -- for debugging


DATA1 is connected to ACSI port
DATA2 is data latched on CS and ACK and connected to MCU

ACSI interface: 
GPIOB_0-7 - IO - DATA2 - ACSI data

GPIOB_8   - I  - RESET - when ACSI is in reset state, shouldn't do anything
GPIOB_9   - I  - CMD - this is combination of CS and A1, will go low on 1st cmd byte from ACSI port -- EXTI9
GPIOB_10  - I  - CS  - PIO strobe                                                                   -- EXTI10
GPIOB_11  - I  - ACK - DMA strobe                                                                   -- EXTI11

//------------
// the following pins have been moved from GPIOB to GPIOA, so we can to direct write to GPIOB->ODR without doing read-modify-write

GPIOA_0   - O  - RnW - defines data direction (1: DATA1 <- DATA2 -- READ,  0: DATA1 -> DATA2 -- WRITE)
GPIOA_1   - O  - PIO - on rising edge will put INT to L
GPIOA_2   - O  - DMA - on rising edge will put DRQ to L
GPIOA_3   - O  - ATN for Attention

//------------

// for the button and LEDs (floppy selection)
GPIOB_12  - I  - BUTTON								-- EXTI12
GPIOA_11  - O  - LED1
GPIOA_12  - O  - LED2 
GPIOA_15  - O  - LED3


*/

// on GPIOB -- inputs
#define aRESET		(1 <<   8)
#define	aCMD			(1 <<   9)
#define	aCS				(1 <<  10)
#define	aACK			(1 <<  11)

#define BUTTON		(1 <<  12)

// on GPIOA - for easier write to aDATA -- outputs
#define	aRNW			(1 <<  0)
#define	aPIO			(1 <<  1)
#define	aDMA			(1 <<  2)
#define	ATN				(1 <<  3)

// on GPIOA - LEDs
#define LED1			(1 << 11)
#define LED2			(1 << 12) 
#define LED3			(1 << 15)

// on GPIOB - XILINX reset
#define XILINX_RESET		(1 << 15)

#endif /* DEFS_H_ */
