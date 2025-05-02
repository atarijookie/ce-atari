#ifndef __COMMAND_HANDLING_H__
#define __COMMAND_HANDLING_H__

#include <arduino.h>

uint8_t idIsEnabled(uint8_t id);

uint8_t onGetCommandAcsi(void);
uint8_t onGetCommandScsi(void);
void getCmdLengthFromCmdBytesAcsi(void);
void getCmdLengthFromCmdBytesScsi(uint8_t cmd);

void onGetCommand(void);
void onDataRead(uint8_t withStatus);
void onDataWrite(void);
void onReadStatus(void);

#endif
