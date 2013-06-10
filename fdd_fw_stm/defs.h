#ifndef DEFS_H_
#define DEFS_H_

#define BYTE	unsigned char
#define WORD	unsigned short
#define DWORD	unsigned int

#define TRUE	1
#define FALSE	0

typedef struct 
{
	WORD buffer[16];					// for 4 ATNs (of length 4 WORDs)
	WORD count;								// count of WORDs in buffer (0 .. 15)
	
	void *next;								// pointer to the next available TAtnBuffer
} TAtnBuffer;

typedef struct 
{
	WORD buffer[550];					// buffer for the written data
	WORD count;								// count of WORDs in buffer 
	
	BYTE readyToSend;					// until we store all the data, don't 
	
	void *next;								// pointer to the next available TAtnBuffer
} TWriteBuffer;

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


inputs:
-------
GPIOB_2  - DIRECTION
GPIOB_3  - STEP							(using EXTI3)
GPIOB_4  - WDATA						(using TIM3_CH1 after remap)
GPIOB_6  - SIDE1
GPIOB_7  - WGATE
GPIOB_12 - MOTOR_ENABLE
GPIOB_13 - DRIVE_SELECT


outputs:
---------
GPIOA_8  - RDATA 						(using TIM1_CH1)

GPIOB_8  - WRITE_PROTECT
GPIOB_9  - DISK_CHANGE
GPIOB_10 - TRACK0
GPIOB_11 - INDEX 						(using TIM2_CH4)
GPIOB_15 - ATTENTION (need more data / data available to retrieve)
*/


// on GPIOA
//#define	RDATA				(1 <<   8)


// on GPIOB
#define	DIR						(1 <<   2)
#define	STEP					(1 <<   3)
#define	WDATA					(1 <<   4)
#define	SIDE1					(1 <<   6)
#define	WGATE					(1 <<   7)

#define	WR_PROTECT		(1 <<   8)
#define	DISK_CHANGE		(1 <<   9)
#define	TRACK0				(1 <<  10)
//#define	INDEX					(1 <<  11)

#define	MOTOR_ENABLE	(1 <<  12)
#define	DRIVE_SELECT	(1 <<  13)

#define	ATN						(1 <<  15)


#endif /* DEFS_H_ */
