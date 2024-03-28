#ifndef __PLAYBACK_H__
#define __PLAYBACK_H__

#include <stdint.h>

// Maximum ACSI sectors we can transfer in one shot is 254 sectors == 130048 bytes.
// One video frame is 32032 bytes, so maximum transfer is 130048 / 32032 = 4 video frames.
#define VIDEO_FPS           20          // FPS of video we want to show
#define ACSI_MAX_VIDEO_FPT  4           // how many video frames can fit in single biggest ACSI transfer (FPT == frames per transfer)
#define VIDEO_FRAME_SIZE    32032       // 32 bytes for palette, 32000 bytes for video data
#define VIDEO_BUFFER_SIZE   (ACSI_MAX_VIDEO_FPT * VIDEO_FRAME_SIZE)

#define AUDIO_PPS           2           // how many parts we will split the audio into every second
#define AUDIO_RATE          25000       // Hz
#define AUDIO_CHANNELS      1           // mono
#define AUDIO_FPT           (VIDEO_FPS / AUDIO_PPS)     // audio frames per single transfer = video fps / how many parts we will split the audio to
#define AUDIO_PART_SIZE     ((AUDIO_RATE * AUDIO_CHANNELS) / AUDIO_PPS)

// current machine details needed for video displaying
typedef struct {
    uint8_t type;           // one of the MACHINE_* values
    uint8_t resolution;     // one of the VID_RES_ST_* values
    uint8_t paletteType;    // one of the VID_PALETTE_* values
} TMachine;

void playback(void);

#endif
