//--------------------------------------------------
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

#define BUFFER_SIZE (1024 + 2)       // extra 2 bytes, because pBuffer will be aligned to even address
uint8_t buffer[BUFFER_SIZE];
uint8_t *pBuffer;

// Maximum ACSI sectors we can transfer in one shot is 254 sectors == 130048 bytes.
// One video frame is 32032 bytes, so maximum transfer is 130048 / 32032 = 4 video frames.
#define VIDEO_FPS           20          // FPS of video we want to show
#define ACSI_MAX_VIDEO_FPT  4           // how many video frames can fit in single biggest ACSI transfer (FPT == frames per transfer)
#define VIDEO_FRAME_SIZE    32032       // 32 bytes for palette, 32000 bytes for video data
#define VIDEO_BUFFER_SIZE   (ACSI_MAX_VIDEO_FPT * VIDEO_FRAME_SIZE)
uint8_t videoBuffer[VIDEO_BUFFER_SIZE + 1 + 512];     // +1 byte to align to even address, +1 sector to make sure that last sector read doesn't overflow the boundary
uint8_t* pVideoBuffer;

#define AUDIO_PPS           2           // how many parts we will split the audio into every second
#define AUDIO_RATE          25000       // Hz
#define AUDIO_CHANNELS      1           // mono
#define AUDIO_FPT           (VIDEO_FPS / AUDIO_PPS)     // audio frames per single transfer = video fps / how many parts we will split the audio to
#define AUDIO_PART_SIZE     ((AUDIO_RATE * AUDIO_CHANNELS) / AUDIO_PPS)
uint8_t audioBuffer[AUDIO_PART_SIZE + 1 + 512];   // +1 byte to align to even address, +1 sector to make sure that last sector read doesn't overflow the boundary
uint8_t* pAudioBuffer;

// create buffer pointer to even address 
uint8_t* addrToEven(uint8_t* addrIn)
{
    uint32_t toEven = (uint32_t) addrIn;

    if(toEven & 1) {        // not even number?
        toEven++;
    }

    return (uint8_t*) toEven; 
}

uint8_t videoRes, videoPalType;

void getMachineDetails(void)
{
    // TODO: fetch machine and palette type
    videoRes = 0;       // one of the VID_RES_ST_* values
    videoPalType = 0;   // one of the AUDIO_* values
}

//--------------------------------------------------
int main(void)
{
    pBuffer = addrToEven(buffer);
    pVideoBuffer = addrToEven(videoBuffer);
    pAudioBuffer = addrToEven(audioBuffer);

    getMachineDetails();    // fill vars about this machine which we need for video playback

    Clear_home();

    //------
    // Before calling any function from the extension, you first must successfully open the extension first.
    // It may take several seconds / minutes, because the extension might be downloaded from internet,
    // it might be compiled or other preparations might be done on first run, and they it takes a little time
    // for it to start. Make sure you either specify long enough timeout time, or try to multiple times.
    (void) Cconws("Opening extension\r\n");
    uint8_t extId = cexOpen("ext_vidaud", "", 5);        // open extension

    if(extId > ANY_ERROR) {     // on error, fail
        (void) Cconws("Opening extension failed (press any key)\r\n");
        sleep(3);
        (void) Cnecin();
        return 0;
    }

    //------
    uint8_t res = cexCallLong(extId, "start", 6, VIDEO_FPS, videoRes, videoPalType, AUDIO_RATE, AUDIO_CHANNELS, "/tmp/bad_apple.mp4");

    if(res != STATUS_OK) {      // calling function failed?
        (void) Cconws("start - call failed\r\n");
        return 0;
    }

    res = cexCallRawRead(extId, "get_frames", ACSI_MAX_VIDEO_FPT, 0, VIDEO_BUFFER_SIZE, pVideoBuffer);

    if(res != STATUS_OK) {
        (void) Cconws("get_frames failed\r\n");
    }

    res = cexCallRawRead(extId, "get_samples", AUDIO_FPT, 0, AUDIO_PART_SIZE, pAudioBuffer);

    if(res != STATUS_OK) {
        (void) Cconws("get_samples failed\r\n");
    }

    //------
    cexCallRawWrite(extId, "stop", 0, 0, 0, pBuffer);   // stop the stream if still running
    cexClose(extId);                                    // close extension

    (void) Cconws("Terminatimg (press any key)\r\n");
    sleep(3);
    (void) Cnecin();
    return 0;
}
