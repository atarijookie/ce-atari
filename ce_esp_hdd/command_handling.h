#ifndef __COMMAND_HANDLING_H__
#define __COMMAND_HANDLING_H__

#include <arduino.h>

uint8_t idIsEnabled(uint8_t id);

uint8_t onGetCommandAcsi(void);
uint8_t onGetCommandScsi(void);
void getCmdLengthFromCmdBytesAcsi(void);
void getCmdLengthFromCmdBytesScsi(uint8_t cmd);

uint8_t onGetCommand(void);
uint8_t onDataRead(uint8_t withStatus);
uint8_t onDataWrite(void);
void onReadStatus(void);

#endif
