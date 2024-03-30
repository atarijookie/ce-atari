#ifndef __MAIN_H__
#define __MAIN_H__

#include <stdint.h>
#include <stdio.h>

#include "stdlib.h"
#include "extension.h"
#include "stream.h"
#include "playback.h"
#include "video.h"
#include "audio.h"

#define BUFFER_SIZE (1024 + 2)       // extra 2 bytes, because pBuffer will be aligned to even address
extern TMachine machine;
extern uint8_t extId;

#define REG_VIDEO_SHIFTER_SYNC_MODE  ((uint8_t*) 0xffff820a)

void getScreenRateFromSyncMode(void);
void getMachineDetails(void);

#endif
