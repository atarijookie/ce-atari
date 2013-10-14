#include "mydefines.h"
//-----------------------------------------------
// bridge functions return value
#define	E_TimeOut			0
#define	E_OK					1
#define	E_OK_A1				2
#define	E_CARDCHANGE	3	
#define	E_RESET				4	

//----------------------------
// bridge function declarations
	
// send data from device to ST in PIO mode
BYTE PIO_read(BYTE byte);

// get data from ST for the device in PIO mode
BYTE PIO_write(void);

// send data from device to ST in DMA mode
BYTE DMA_read(BYTE byte);
BYTE DMA_readFast(BYTE byte);

// get data from ST for the device in DMA mode
BYTE DMA_write(void);

BYTE DMA_writeFast(void);
BYTE DMA_writeFastPost(void);

BYTE GetCmdByte(void);

// set-up functions of bridge
void PreDMA_read(void);
void PostDMA_read(void);
void PreDMA_write(void);
void PostDMA_write(void);

//---------------------------------------
/*
char Read512(void);						 	// ST reads 512 bytes from buffer in Atmel  
char Write512(void);						// ST writes 512 bytes to buffer in Atmel
*/
//---------------------------------------
 
