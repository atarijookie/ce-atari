#ifndef __AUDIO_H__
#define __AUDIO_H__

#include <stdint.h>
#include "playback.h"

#define AUDIO_PPS               2                           // how many parts we will split the audio into every second
#define AUDIO_FPT               (VIDEO_FPS / AUDIO_PPS)     // audio frames per single transfer = video fps / how many parts we will split the audio to
#define STE_PLAYBACK_RATE_HZ    25033

typedef struct {
    uint16_t channels;
    uint16_t rateHz;
    uint16_t partSize;
} TAudioParams;

extern TAudioParams audio;

void audioInitParams(void);
void audioPlay(void);
void audioStop(void);
void audioCheckAndFeed(void);

void audioGetSamplesFirstTime(void);
void audioGetSamples(uint8_t* pStoreNewSamples, uint32_t bytesWant);

#endif
