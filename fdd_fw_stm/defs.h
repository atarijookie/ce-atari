#ifndef DEFS_H_
#define DEFS_H_

#define BYTE	unsigned char
#define WORD	unsigned short
#define DWORD	unsigned int

#define TRUE	1
#define FALSE	0

/*
inputs:
PIO2_0  - MOTOR_ENABLE
PIO2_1  - DRIVE_SELECT
PIO2_2  - DIRECTION
PIO2_3  - STEP
PIO2_4  - WDATA
PIO2_5  - WGATE
PIO2_6  - SIDE1

outputs:
PIO2_7  - INDEX
PIO2_8  - TRACK0
PIO2_9  - RDATA
PIO2_10 - WRITE_PROTECT
PIO2_11 - DISK_CHANGE

PIO3_0  - ATTENTION (need more data)

temp inputs on counters:
PIO1_5 - RDATA
PIO1_8 - WDATA
*/

// on PIO2
#define	MOTOR_ENABLE	(1 <<  0)
#define	DRIVE_SELECT	(1 <<  1)
#define	DIR				(1 <<  2)
#define	STEP			(1 <<  3)
#define	WDATA			(1 <<  4)
#define	WGATE			(1 <<  5)
#define	SIDE1			(1 <<  6)
#define	INDEX			(1 <<  7)
#define	TRACK0			(1 <<  8)
#define	RDATA			(1 <<  9)
#define	WR_PROTECT		(1 << 10)
#define	DISK_CHANGE		(1 << 11)

// on PIO3
#define	ATN				(1 <<  0)

// temp inputs on PIO1
#define	RDATAtmp		(1 <<  5)
#define	WDATAtmp		(1 <<  8)


#endif /* DEFS_H_ */
