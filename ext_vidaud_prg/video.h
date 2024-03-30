#ifndef __VIDEO_H__
#define __VIDEO_H__

#include <stdint.h>
#include "fifo.h"

// Maximum ACSI sectors we can transfer in one shot is 254 sectors == 130048 bytes.
// One video frame is 32032 bytes, so maximum transfer is 130048 / 32032 = 4 video frames.
#define VIDEO_FPS           20          // FPS of video we want to show
#define ACSI_MAX_VIDEO_FPT  4           // how many video frames can fit in single biggest ACSI transfer (FPT == frames per transfer)
#define VIDEO_FRAME_SIZE    32032       // 32 bytes for palette, 32000 bytes for video data
#define VIDEO_BUFFER_SIZE   (ACSI_MAX_VIDEO_FPT * VIDEO_FRAME_SIZE)

uint8_t *getCurrentVideoAddr(void);
void setCurrentVideoAddr(uint8_t *pData);
void showFrame(void);
void vblInstallHandler(void);
void vblRemoveHandler(void);
void storeCurrentScreen(void);
void restoreCurrentScreen(void);
void getVideoFrames(void);
void videoInit(void);

extern Fifo videoFifo;

#endif
