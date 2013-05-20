#ifndef DEFS_H_
#define DEFS_H_

#define BYTE	unsigned char
#define WORD	unsigned short
#define DWORD	unsigned int

#define TRUE	1
#define FALSE	0

/*
reserved:
---------
GPIOA_4 - SPI
GPIOA_5 - SPI
GPIOA_6 - SPI
GPIOA_7 - SPI

GPIOA_13 - SWD
GPIOA_14 - SWD


inputs:
-------
GPIOB_0  - MOTOR_ENABLE
GPIOB_1  - DRIVE_SELECT
GPIOB_2  - DIRECTION
GPIOB_3  - STEP
GPIOB_4  - WDATA
GPIOB_5  - WGATE
GPIOB_6  - SIDE1


outputs:
---------
GPIOA_8 - INDEX (using TIM1_CH1)

GPIOB_8  - WRITE_PROTECT
GPIOB_9  - DISK_CHANGE
GPIOB_10 - TRACK0


PIO2_9  - RDATA
PIO3_0  - ATTENTION (need more data)

*/

// on PIO2
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

#define	RDATA					(1 <<  9)

// on PIO3
#define	ATN				(1 <<  0)

// temp inputs on PIO1
#define	RDATAtmp		(1 <<  5)
#define	WDATAtmp		(1 <<  8)


#endif /* DEFS_H_ */
