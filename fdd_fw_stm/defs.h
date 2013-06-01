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
	WORD sending;							// mark this struct as currently sending
	
	void *next;								// pointer to the next available TAtnBuffer
} TAtnBuffer;

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
GPIOB_0  - MOTOR_ENABLE
GPIOB_1  - DRIVE_SELECT
GPIOB_2  - DIRECTION
GPIOB_3  - STEP										// EXTI3
GPIOB_4  - WDATA
GPIOB_5  - WGATE
GPIOB_6  - SIDE1


outputs:
---------
GPIOA_8  - RDATA (using TIM1_CH1)

GPIOB_8  - WRITE_PROTECT
GPIOB_9  - DISK_CHANGE
GPIOB_10 - TRACK0
GPIOB_11 - INDEX (using TIM2_CH4)
GPIOB_15 - ATTENTION (need more data / data available to retrieve)
*/

// on GPIOB
#define	MOTOR_ENABLE	(1 <<  0)
#define	DRIVE_SELECT	(1 <<  1)
#define	DIR						(1 <<  2)
#define	STEP					(1 <<  3)
#define	WDATA					(1 <<  4)
#define	WGATE					(1 <<  5)
#define	SIDE1					(1 <<  6)

#define	WR_PROTECT		(1 <<   8)
#define	DISK_CHANGE		(1 <<   9)
#define	TRACK0				(1 <<  10)
#define	RDATA					(1 <<  11)


#define	ATN						(1 <<  15)


#endif /* DEFS_H_ */
