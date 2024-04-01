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
#include "video.h"
#include "audio.h"

uint8_t streamEOF = 0;  // non-zero if we did reach end of stream

void playback(void)
{
    streamEOF = 0;          // not EOF at the start

    // get first video frames and audio samples, then start playing
    if(play.audio) {
        audioGetSamplesFirstTime();
    }

    if(play.video) {
        videoInit();
        getVideoFrames();
        vblInstallHandler();
    }

    // start video and audio playback
    if(play.audio) {
        audioPlay();
    }

    // keep playing while not at the end of stream and we still got some frames
    while(!streamEOF && videoFifo.count > 0) {
        if(play.video) {
            // We are using 2 buffers for receiving frames, each fits ACSI_MAX_VIDEO_FPT frames into it.
            // If we're completelly full, fifo holds 2*ACSI_MAX_VIDEO_FPT frames, and if we're 
            // half-full then fifo has ACSI_MAX_VIDEO_FPT frames in it (or less). This is the moment we
            // can fetch more frames, because this means that one of the buffers is empty.
            if(!streamEOF && videoFifo.count <= ACSI_MAX_VIDEO_FPT) {
                getVideoFrames();
            }
        }

        if(play.audio) {
            // call this periodically to check if the audio stream needs refilling and feed it
            audioCheckAndFeed();
        }
    }
    
    // stop audio and video playback
    if(play.audio) {
        audioStop();
    }

    if(play.video) {
        vblRemoveHandler();
        Supexec(restoreCurrentScreen);  // restore original screen content with palette
    }
}
