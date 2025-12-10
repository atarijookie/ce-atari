#ifndef __COMMAND_HANDLING_H__
#define __COMMAND_HANDLING_H__

#include <arduino.h>

uint8_t idIsEnabled(uint8_t id);

uint8_t onGetCommandAcsi(void);
uint8_t onGetCommandScsi(void);
void getCmdLengthFromCmdBytesAcsi(void);
void getCmdLengthFromCmdBytesScsi(uint8_t cmd);

uint8_t onGetCommand(void);
bool onDataRead(uint32_t cnt, uint8_t* bfr);
bool onDataWrite(uint32_t dataCnt);
void onReadStatus(uint8_t statusByte);

#endif
