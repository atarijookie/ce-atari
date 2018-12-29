#ifndef DEFS_H_
#define DEFS_H_

#define BYTE    unsigned char
#define WORD    unsigned short
#define DWORD   unsigned int

#define TRUE    1
#define FALSE   0

#define STREAM_TABLE_ITEMS  20
#define STREAM_TABLE_SIZE   (2 * STREAM_TABLE_ITEMS)

#define WRITEBUFFER_SIZE        650

typedef struct 
{
    WORD buffer[WRITEBUFFER_SIZE];  // buffer for the written data
    WORD count;                                         // count of WORDs in buffer 
    
    BYTE readyToSend;                               // until we store all the data, don't 
    
    void *next;                                         // pointer to the next available TAtnBuffer
} TWriteBuffer;

typedef struct {
    BYTE track;
    BYTE side;
} TDrivePosition;   

typedef struct {
    BYTE side;
    BYTE track;
    BYTE sector;
} SStreamed;

#define CIRCBUFFER_SIZE		64
#define CIRCBUFFER_POSMASK	0x3f

typedef struct {
	BYTE addPos;
	BYTE getPos;
	BYTE count;

	BYTE data[CIRCBUFFER_SIZE];
} TCircBuffer;

typedef struct {
    BYTE stWantsTheStream;          // based on floppy SELECT0/1 signal and MOTOR ON signal
    BYTE weAreReceivingTrack;       // based on if STEP pulse happened, if SPI is receiving the track or not

    BYTE updatePosition;
    
    BYTE outputsAreEnabled;         // this says whether currently the output pins are streaming the MFM stream or not    
} TOutputFlags;

/*
reserved:
---------
GPIOA_2  - USART2_TX -- for IKBD
GPIOA_3  - USART2_RX -- for IKBD

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
GPIOB_3  - STEP                         (using EXTI3)
GPIOB_4  - WDATA                        (using TIM3_CH1 after remap)
GPIOB_6  - SIDE1
GPIOB_7  - WGATE
GPIOB_12 - MOTOR_ENABLE
GPIOB_13 - DRIVE_SELECT0
GPIOB_14 - DRIVE_SELECT1


outputs:
---------
GPIOA_1  - INDEX                        (using TIM2_CH2)

GPIOA_8  - RDATA                        (using TIM1_CH1)

GPIOB_8  - WRITE_PROTECT
GPIOB_9  - DISK_CHANGE
GPIOB_10 - TRACK0
GPIOB_11 - USART3_RX -- for original ST keyboard TX
GPIOB_15 - ATTENTION (need more data / data available to retrieve)
*/


// on GPIOA
//#define   RDATA               (1 <<   8)


// on GPIOB
#define DIR             (1 <<   2)
#define STEP            (1 <<   3)
#define WDATA           (1 <<   4)
#define SIDE1           (1 <<   6)
#define WGATE           (1 <<   7)

#define WR_PROTECT      (1 <<   8)
#define DISK_CHANGE     (1 <<   9)
#define TRACK0          (1 <<  10)

#define MOTOR_ENABLE    (1 <<  12)
#define DRIVE_SELECT0   (1 <<  13)
#define DRIVE_SELECT1   (1 <<  14)

#define ATN             (1 <<  15)


#endif /* DEFS_H_ */
