#ifndef __MFM_H__
#define __MFM_H__

#include <arduino.h>

#define FILL_NONE   0
#define FILL_LOWER  1
#define FILL_UPPER  2

extern volatile uint8_t fillWhat;

void setupPwmOutput(void);
void setupDmaToPwm(void);

void fillHalfMfmBuffer(void);

#endif
