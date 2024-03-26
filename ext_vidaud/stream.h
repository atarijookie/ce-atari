#ifndef __STREAM_H__
#define __STREAM_H__

#include "extensiondefs.h"
#include "main.h"

#define RGB_FRAME_SIZE_BYTES    (640*400*3)

#define VID_PALETTE_NONE    0
#define VID_PALETTE_ST      1       // 9 bits per color
#define VID_PALETTE_STE     2       // 12 bits per color

#define VID_RES_OFF     0
#define VID_RES_ST_LOW  1
#define VID_RES_ST_MID  2
#define VID_RES_ST_HIGH 3

#define AUDIO_OFF       0
#define AUDIO_MONO      1
#define AUDIO_STEREO    2

#define STATUS_NO_MORE_FRAMES   0xF0

struct TStream {
    uint8_t videoFps;
    uint8_t videoResolution;
    uint8_t videoPaletteType;
    uint16_t audioRateHz;
    uint8_t audioChannels;
    std::string filePath;
    bool running = false;
};

class Fifo;

void exportFunctionSignatures(void);
const char *getResolutionString(uint8_t resolution);
const char *getPixelFormat(uint8_t resolution);
void createShellCommand(char *cmdBuffer, int cmdBufferLen, const char *inputFile, uint8_t vidFps, uint8_t vidRes, uint16_t audioRate, uint16_t audioChannels);
bool waitForBytesInFifo(Fifo *fifo, uint32_t bytesWant, uint32_t waitFrames);
void convertVideoFrameToSt(uint8_t *frameDataRGB, uint32_t videoBytesPerFrame, uint8_t *stFrame);

#endif
