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

void resetBridge(void);
void getBridgeStatus(void);
uint8_t isBusIdle(void);

uint8_t PIO_gotFirstCmdByte(void); // check if we got the 1st command byte
uint8_t PIO_writeFirst(void);      // get 1st CMD byte from ST  -- without setting INT
uint8_t PIO_write(void);           // get next CMD byte from ST -- with setting INT to LOW and waiting for CS

void PIO_read(uint8_t scsiStatusByte); // send status byte to ST

uint8_t DMA_write(void);    // get byte from ST using DMA
void DMA_read(uint8_t val); // send byte to ST using DMA

void MSG_read(uint8_t val);
void PIO_read_solely(uint8_t val);

// internal functions
void setDataDirection(uint8_t sendNotRecv);
uint8_t waitForEOT(void);
void waitForEOTlevel(int level);
uint8_t dataIn(void);
void dataOut(uint8_t data);

void dumpPinStates(void);

#endif
