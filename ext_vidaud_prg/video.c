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
#include "video.h"
#include "fifo.h"
#include "main.h"

//--------
// The video buffers are used for receiving of the video frames and also for showing the video frames,
// so while one is being used for showing, the other will be used for receiving.

// +256 because ST video RAM has to be aligned to multiple of 256 (no Video Base Address Low register)
// +1 uint8_t to align to even address, +1 sector to make sure that last sector read doesn't overflow the boundary
uint8_t videoBuffer1[256 + VIDEO_BUFFER_SIZE + 1 + 512];
uint8_t videoBuffer2[256 + VIDEO_BUFFER_SIZE + 1 + 512];
uint8_t* pVideoBuffer[2];

int videoIndexGet = -1;

#define ST_PALETTE_SIZE     32
#define ST_VIDEODATA_SIZE   32000

uint8_t* pOriginalScreen;
uint8_t originalPalette[ST_PALETTE_SIZE];
uint8_t originalVideoData[ST_VIDEODATA_SIZE];

#define REG_VIDEOBASE_HIGH  ((uint8_t*) 0xffff8201)
#define REG_VIDEOBASE_MID   ((uint8_t*) 0xffff8203)
#define REG_VIDEOBASE_LOW   ((uint8_t*) 0xffff820D)     // only on STE

#define REG_VIDEO_PALETTE   ((uint16_t*) 0xffff8240)

Fifo videoFifo;

#define VBL_COUNTER_MULTIPLIER  1000000
uint32_t vblCounter;
uint32_t vblCounterStartValue;

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

// To show next frame, add pointer to palette and video data to fifo and this function will pick it up and show it
void showFrame(void)
{
    uint32_t palette, videoData;
    fifoGet(&videoFifo, &palette, &videoData);  // fetch data from Fifo

    if(palette == 0 || videoData == 0) {        // no data? no change
        return;
    }

    if(machine.resolution != VID_RES_ST_HIGH) {     // don't set palette on ST high
        memcpy(REG_VIDEO_PALETTE, (uint8_t*) palette, ST_PALETTE_SIZE);   // set new palette
    }

    setCurrentVideoAddr((uint8_t*) videoData);                        // set video register to next video data
}

void install_vbl_routine(void);     // this is defined in screenirq.s
void uninstall_vbl_routine(void);   // this is defined in screenirq.s

void vblInstallHandler(void)
{
    /*
    Before installing the vbl routine, we need to initialize the vbl counter and related vars.
    The count of VBLs to skip to achieve desired video fps rate might be a rational number,
    e.g. for 50 Hz refresh rate and 20 fps of video, you need to show a frame every 50/20 = 2.5 vbl,
    which is not exactly possible. So instead, do a countdown from the exact frames-per-vbl down to 0,
    and when the remaining frame count to skip is less than 1, then show the frame and add that original 
    skip rate (2.5) to the skip counter. This will result frames being shown after 2 vbls, 
    then after 3 vbls, and then again 2, 3, 2, 3, which translates roughly to 2.5 vbl (or any other 
    skip rate that is rational and gets calculated). We don't want to do this on floats, so we will
    use integers - multiplied by million (so even 70'000'000 fits into uint32_t).
    */
    vblCounterStartValue = (((uint32_t) machine.screenRateHz) * VBL_COUNTER_MULTIPLIER) / VIDEO_FPS;
    vblCounter = vblCounterStartValue;

    // install the vbl routine
    Supexec(install_vbl_routine);
}

void vblRemoveHandler(void)
{
    Supexec(uninstall_vbl_routine);
}

void vblHandlerInC(void)
{
    // No frames in video fifo means that there's nothing more to show (end of stream),
    // or the next frame is not received and we're too late. Either way, nothing to show.
    if(videoFifo.count == 0) {
        return;
    }

    vblCounter -= VBL_COUNTER_MULTIPLIER;       // decrease counter by 1.0 (on uint32_t)

    // the vbl counter is >= 1.0, so there's one more vbl to skip - don't shot frame
    if(vblCounter >= VBL_COUNTER_MULTIPLIER) {
        return;
    }
    
    // it's time to show the frame
    vblCounter += vblCounterStartValue; // add starting value to vbl counter, so the rest bellow 1.0 doesn't get lost
    showFrame();
}

// preserve current screen content with palette
void storeCurrentScreen(void)
{
    uint8_t* pScreen = getCurrentVideoAddr();   // where HW points that the screen is
    pOriginalScreen = pScreen;                  // store for later restoring

    if(machine.resolution != VID_RES_ST_HIGH) {     // don't save palette on ST high
        memcpy(originalPalette, REG_VIDEO_PALETTE, ST_PALETTE_SIZE);  // save palette
    }

    memcpy(originalVideoData, pScreen, ST_VIDEODATA_SIZE);  // save videodata
}

// restore original screen content with palette
void restoreCurrentScreen(void)
{
    memcpy(pOriginalScreen, originalVideoData, ST_VIDEODATA_SIZE);  // restore original video data to original screen

    fifoInit(&videoFifo);   // clear the fifo to remove any existing frames from it
    fifoAdd(&videoFifo, (uint32_t) originalPalette, (uint32_t) pOriginalScreen);

    showFrame();
}

void getVideoFrames(void)
{
    // update index for get (that ++ and if allows us to start from -1 which is no-previous-frames)
    videoIndexGet++;
    if(videoIndexGet > 1) {     // we got only 2 buffers, so go back to 0 after index overflow
        videoIndexGet = 0;
    }

    uint8_t res = cexCallRawRead(extId, "get_frames", ACSI_MAX_VIDEO_FPT, 0, VIDEO_BUFFER_SIZE, pVideoBuffer[videoIndexGet]);

    if(res == STATUS_NO_MORE_FRAMES) {  // end of stream? we're done
        streamEOF = 1;
        return;
    }

    // The status byte of 'get_frames' function is the count of frames the returned data holds.
    // If no frames were received or it's more then requested-frames-count, then it's an error 
    // and we should not handle the content.
    if(res < 1 || res > ACSI_MAX_VIDEO_FPT) {
        // (void) Cconws("get_frames failed\r\n");
        return;
    }

    int i;
    uint32_t framesReceived = res;
    uint8_t* pVideoData = pVideoBuffer[videoIndexGet];                      // start of video data
    uint8_t* pPalette = pVideoData + (framesReceived * ST_VIDEODATA_SIZE);  // start of palettes

    for(i=0; i<framesReceived; i++) {               // put all the frames into video fifo
        fifoAdd(&videoFifo, (uint32_t) pVideoData, (uint32_t) pPalette);
        pVideoData += ST_VIDEODATA_SIZE;            // move to next frame
        pPalette += ST_PALETTE_SIZE;                // move to next palette
    }
}

void videoInit(void)
{
    fifoInit(&videoFifo);           // init the video fifo
    Supexec(storeCurrentScreen);    // preserve current screen content with palette

    pVideoBuffer[0] = addrToLowestByteZero(videoBuffer1);
    pVideoBuffer[1] = addrToLowestByteZero(videoBuffer2);

    videoIndexGet = -1;     // start with -1 here to do the 1st get into 0th buffer
}
