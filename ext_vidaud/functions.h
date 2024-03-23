#ifndef __FUNCTIONS_H__
#define __FUNCTIONS_H__

#include "extensiondefs.h"
#include "main.h"

#define VID_RES_OFF     0
#define VID_RES_ST_LOW  1
#define VID_RES_ST_MID  2
#define VID_RES_ST_HIGH 3

#define AUDIO_OFF       0
#define AUDIO_MONO      1
#define AUDIO_STEREO    2

#define STATUS_NO_MORE_FRAMES   0xF0

void exportFunctionSignatures(void);

#endif
