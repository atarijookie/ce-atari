#ifndef __BRIGDE_H__
#define __BRIGDE_H__

#include "defs.h"

// bridge functions return value
#define E_TimeOut 0
#define E_OK 1
#define E_OK_A1 2
#define E_CARDCHANGE 3
#define E_RESET 4
#define E_FAIL_CMD_AGAIN 5

#define DIR_RECV 0
#define DIR_SEND 1

#define MODE_UNKNOWN            0
#define MODE_RESET              1
#define MODE_SCSI_SELECTION     2
#define MODE_CMD                3
#define MODE_MSG_OUT            4
#define MODE_DMA_READ           5
#define MODE_DMA_WRITE          6
#define MODE_STATUS             7
#define MODE_MSG_IN             8

void pioConfigAll(void);
void pioConfig(int newMode, bool force=false);

void resetBridge(void);

uint8_t isSelectionHappening(void); // check if we got the 1st command byte
uint8_t getSelectionByte(void);      // get 1st CMD byte from ST  -- without setting INT
uint8_t PIO_write(void);           // get next CMD byte from ST -- with setting INT to LOW and waiting for CS

void statusAndMsgRead(uint8_t scsiStatusByte); // send status byte to ST

void DMA_write_startWithCount(uint32_t transfersCount);     // set how many bytes we will transfer, before calling DMA_write()
uint8_t DMA_write(void);    // get byte from ST using DMA

void DMA_read(uint8_t val);         // send byte to ST using DMA
void DMA_read_waitForEnd(void);

void PIO_read(uint8_t val);

// internal functions
void dumpPinStates(void);

#endif
