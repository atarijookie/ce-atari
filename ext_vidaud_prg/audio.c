#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <mint/linea.h>
#include <support.h>

#include <stdint.h>
#include <stdio.h>

#include "stdlib.h"
#include "extension.h"
#include "stream.h"
#include "playback.h"
#include "fifo.h"
#include "audio.h"
#include "main.h"

TAudioParams audio;

#define AUDIO_PLAY_SIZE 55000
uint8_t audioBuffPlay[AUDIO_PLAY_SIZE]; // up to 1 second of data at the maximum rate
uint8_t* pAudioBuffPlayStart;
uint8_t* pAudioBuffPlayHalf;
uint8_t* pAudioBuffPlayEnd;

#define AUDIO_RECV_SIZE     (AUDIO_PLAY_SIZE / 2)
uint8_t audioBuffRecv[AUDIO_RECV_SIZE];
uint8_t* pAudioBuffRecv;

#define REG_AUDIO_STE_CTRL          ((uint8_t*) 0xffff8901)
#define REG_AUDIO_STE_START_HIGH    ((uint8_t*) 0xffff8903)
#define REG_AUDIO_STE_START_MID     ((uint8_t*) 0xffff8905)
#define REG_AUDIO_STE_START_LOW     ((uint8_t*) 0xffff8907)
#define REG_AUDIO_STE_COUNTER_HIGH  ((uint8_t*) 0xffff8909)
#define REG_AUDIO_STE_COUNTER_MID   ((uint8_t*) 0xffff890B)
#define REG_AUDIO_STE_COUNTER_LOW   ((uint8_t*) 0xffff890D)
#define REG_AUDIO_STE_END_HIGH      ((uint8_t*) 0xffff890F)
#define REG_AUDIO_STE_END_MID       ((uint8_t*) 0xffff8911)
#define REG_AUDIO_STE_END_LOW       ((uint8_t*) 0xffff8913)
#define REG_AUDIO_STE_CHANNELS_RATE ((uint8_t*) 0xffff8921)

void audioInitParams(void)
{
    audio.channels = 1;
    audio.rateHz = (machine.type == MACHINE_STE) ? STE_PLAYBACK_RATE_HZ : 15000;   // playback rate for STE or ST
    audio.partSize = (audio.rateHz * audio.channels) / AUDIO_PPS;   // how many bytes will be one part of samples received

    // to even address
    pAudioBuffPlayStart = addrToEven(audioBuffPlay);
    pAudioBuffRecv = addrToEven(audioBuffRecv);
}

void audioSTEsetStart(uint8_t* pData)
{
    uint32_t pDataInt = (uint32_t) pData;
    uint32_t high = (pDataInt >> 16) & 0xff;
    uint32_t mid = (pDataInt >> 8) & 0xff;
    uint32_t low = pDataInt & 0xff;

    *REG_AUDIO_STE_START_HIGH = high;
    *REG_AUDIO_STE_START_MID = mid;
    *REG_AUDIO_STE_START_LOW = low;
}

void audioSTEsetEnd(uint8_t* pData)
{
    uint32_t pDataInt = (uint32_t) pData;
    uint32_t high = (pDataInt >> 16) & 0xff;
    uint32_t mid = (pDataInt >> 8) & 0xff;
    uint32_t low = pDataInt & 0xff;

    *REG_AUDIO_STE_END_HIGH = high;
    *REG_AUDIO_STE_END_MID = mid;
    *REG_AUDIO_STE_END_LOW = low;
}

uint8_t* audioSTEgetCounter(void)
{
    uint32_t high = ((*REG_AUDIO_STE_COUNTER_HIGH) & 0xff);
    uint32_t mid = ((*REG_AUDIO_STE_COUNTER_MID) & 0xff);
    uint32_t low = ((*REG_AUDIO_STE_COUNTER_LOW) & 0xff);
    uint32_t addr = (high << 16) | (mid << 8) | low;
    return (uint8_t*)addr;
}

void audioPlaySTE(void)
{
    pAudioBuffPlayEnd = pAudioBuffPlayStart + STE_PLAYBACK_RATE_HZ;
    pAudioBuffPlayHalf = pAudioBuffPlayStart + (STE_PLAYBACK_RATE_HZ/2);

    audioSTEsetStart(pAudioBuffPlayStart);
    audioSTEsetEnd(pAudioBuffPlayEnd);
    *REG_AUDIO_STE_CHANNELS_RATE = 0x82;    // 10 0000 10 ====> 10 = 8-bit Mono, 10 = 25033 Hz
    *REG_AUDIO_STE_CTRL = 0x03;             // Repeat Playback 1, Playback Enable 1
}

void audioStopSTE(void)
{
    *REG_AUDIO_STE_CTRL = 0;                // Repeat Playback 0, Playback Enable 0
}

uint8_t wasInFirstPart;

void audioSTEcheckAndFeed(void)
{
    uint8_t* pNow = audioSTEgetCounter();   // get where the audio playback is now

    uint8_t isInFirstPart = pNow < pAudioBuffPlayHalf;

    if(isInFirstPart == wasInFirstPart) {   // if no change of in which part the playback is, we can quit
        return;
    }

    // if got here, playback went from 1st part to 2nd (half-complete) or from 2nd part to 1st (full-complete)
    wasInFirstPart = isInFirstPart;         // remember this for next check
    uint8_t* pStoreNewSamples;

    if(isInFirstPart) { // full-complete happened because went from 2nd part to 1st part, we can get samples into 2nd part
        pStoreNewSamples = pAudioBuffPlayHalf;
    } else {            // half-complete happened because went from 1st part to 2nd part, we can get samples into 1st part
        pStoreNewSamples = pAudioBuffPlayStart;
    }

    uint32_t bytesWant = (STE_PLAYBACK_RATE_HZ/2);
    audioGetSamples(pStoreNewSamples, bytesWant);
}

void audioGetSamplesFirstTime(void)
{
    wasInFirstPart = 0; // we're in the 1st part, don't fetch until we get into 2nd part of buffer

    uint32_t bytesWant = audio.rateHz / 2;
    audioGetSamples(pAudioBuffPlayStart, bytesWant);
}

void audioGetSamples(uint8_t* pStoreNewSamples, uint32_t bytesWant)
{
    // fetch data into receiving buffer
    uint8_t res = cexCallRawRead(extId, "get_samples", AUDIO_FPT, 0, bytesWant, pAudioBuffRecv);

    // if no more frames == end of stream, clear the playback buffer part and quit
    if(res == STATUS_NO_MORE_FRAMES) {
        memset(pStoreNewSamples, 0, bytesWant);
        streamEOF = 1;
        return;
    }

    // if status is greated than count of frames we wanted, it's an error
    if(res > AUDIO_FPT) {
        // (void) Cconws("get_samples failed\r\n");
        return;
    }

    // copy in data from receive buffer to circular playback buffer
    memcpy(pStoreNewSamples, pAudioBuffRecv, bytesWant);
}

void audioSTcheckAndFeed(void)
{
    
}

void audioPlayST(void)
{

}

void audioStopST(void)
{

}

void audioPlay(void)
{
    if(machine.type == MACHINE_STE) {
        audioPlaySTE();
    } else {
        audioStopST();
    }
}

void audioStop(void)
{
    if(machine.type == MACHINE_STE) {
        audioStopSTE();
    } else {
        audioStopST();
    }
}

// call this periodically to check if the audio stream needs refilling and feed it
void audioCheckAndFeed(void)
{
    if(machine.type == MACHINE_STE) {
        audioSTEcheckAndFeed();
    } else {
        audioSTcheckAndFeed();
    }
}
