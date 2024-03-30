#ifndef __AUDIO_H__
#define __AUDIO_H__

#include <stdint.h>
#include "playback.h"

#define AUDIO_PPS           2                           // how many parts we will split the audio into every second
#define AUDIO_FPT           (VIDEO_FPS / AUDIO_PPS)     // audio frames per single transfer = video fps / how many parts we will split the audio to

typedef struct {
    uint16_t channels;
    uint16_t rateHz;
    uint16_t partSize;
} TAudioParams;

extern TAudioParams audio;

void audioInitParams(void);

#endif
