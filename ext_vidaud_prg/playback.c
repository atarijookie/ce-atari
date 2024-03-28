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

// +256 because ST video RAM has to be aligned to multiple of 256 (no Video Base Address Low register)
// +1 uint8_t to align to even address, +1 sector to make sure that last sector read doesn't overflow the boundary
uint8_t videoBuffer1[256 + VIDEO_BUFFER_SIZE + 1 + 512];
uint8_t videoBuffer2[256 + VIDEO_BUFFER_SIZE + 1 + 512];
uint8_t* pVideoBuffer[2];

// +1 uint8_t to align to even address, +1 sector to make sure that last sector read doesn't overflow the boundary
uint8_t audioBuffer1[AUDIO_PART_SIZE + 1 + 512];
uint8_t audioBuffer2[AUDIO_PART_SIZE + 1 + 512];
uint8_t* pAudioBuffer[2];

int videoIndexPlay = 0;
int videoIndexGet = 1;
int audioIndexPlay = 0;
int audioIndexGet = 1;

extern TMachine machine;
extern uint8_t extId;

#define ST_PALETTE_SIZE     32
#define ST_VIDEODATA_SIZE   32000

uint8_t* pOriginalScreen;
uint8_t originalPalette[ST_PALETTE_SIZE];
uint8_t originalVideoData[ST_VIDEODATA_SIZE];

uint8_t* pNextPalette;      // what palette should be set next
uint8_t* pNextVideoData;    // what video address should be set next

#define REG_VIDEOBASE_HIGH  ((uint8_t*) 0xffff8201)
#define REG_VIDEOBASE_MID   ((uint8_t*) 0xffff8203)
#define REG_VIDEOBASE_LOW   ((uint8_t*) 0xffff820D)     // only on STE

#define REG_VIDEO_PALETTE   ((uint16_t*) 0xffff8240)

uint8_t* getCurrentVideoAddr(void)
{
    uint32_t videoHigh = ((*REG_VIDEOBASE_HIGH) & 0xff);
    uint32_t videoMid = ((*REG_VIDEOBASE_MID) & 0xff);
    uint32_t videoLow = (machine.type == MACHINE_STE) ? ((*REG_VIDEOBASE_LOW) & 0xff) : 0;  // on ST video low is 0, on STE it's an actual value
    uint32_t videoAddr = (videoHigh << 16) | (videoMid << 8) | videoLow;
    uint8_t* pScreen = (uint8_t*)videoAddr;
    return pScreen;
}

void setCurrentVideoAddr(uint8_t* pData)
{
    uint32_t pDataInt = (uint32_t) pData;
    uint32_t videoHigh = (pDataInt >> 16) & 0xff;
    uint32_t videoMid = (pDataInt >> 8) & 0xff;
    uint32_t videoLow = pDataInt & 0xff;

    *REG_VIDEOBASE_HIGH = videoHigh;
    *REG_VIDEOBASE_MID = videoMid;

    if(machine.type == MACHINE_STE) {
        *REG_VIDEOBASE_LOW = videoLow;
    }
}

// To show next frame, set pointer to pNextPalette to point to palette and pNextVideoData to point where the video data is.
void showFrame(void)
{
    memcpy(REG_VIDEO_PALETTE, pNextPalette, ST_PALETTE_SIZE);   // set new palette
    setCurrentVideoAddr(pNextVideoData);                        // set video register to next video data
}

// preserve current screen content with palette
void storeCurrentScreen(void)
{
    uint8_t* pScreen = getCurrentVideoAddr();   // where HW points that the screen is
    pOriginalScreen = pScreen;                  // store for later restoring

    memcpy(originalPalette,   REG_VIDEO_PALETTE, ST_PALETTE_SIZE);    // save palette
    memcpy(originalVideoData, pScreen,           ST_VIDEODATA_SIZE);  // save videodata
}

// restore original screen content with palette
void restoreCurrentScreen(void)
{
    pNextPalette = originalPalette;                                 // point next palette to the original one
    memcpy(pOriginalScreen, originalVideoData, ST_VIDEODATA_SIZE);  // restore original video data to original screen
    pNextVideoData = pOriginalScreen;                               // point next video data to original screen

    showFrame();
}

void playback(void)
{
    Supexec(storeCurrentScreen);    // preserve current screen content with palette

    pVideoBuffer[0] = addrToLowestByteZero(videoBuffer1);
    pVideoBuffer[1] = addrToLowestByteZero(videoBuffer2);

    pAudioBuffer[0] = addrToEven(audioBuffer1);
    pAudioBuffer[1] = addrToEven(audioBuffer2);

    while(1) {
        // uint8_t res;

        // res = cexCallRawRead(extId, "get_frames", ACSI_MAX_VIDEO_FPT, 0, VIDEO_BUFFER_SIZE, pVideoBuffer[0]);

        // if(res == STATUS_NO_MORE_FRAMES) {  // end of stream? we're done
        //     return;
        // }

        // if(res != STATUS_OK) {
        //     (void) Cconws("get_frames failed\r\n");
        // }

        // res = cexCallRawRead(extId, "get_samples", AUDIO_FPT, 0, AUDIO_PART_SIZE, pAudioBuffer[0]);

        // if(res != STATUS_OK) {
        //     (void) Cconws("get_samples failed\r\n");
        // }
    }

    Supexec(restoreCurrentScreen);    // restore original screen content with palette
}
