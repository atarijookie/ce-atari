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

void audioInitParams(void)
{
    audio.channels = 1;
    audio.rateHz = (machine.type == MACHINE_STE) ? 25033 : 15000;   // playback rate for STE or ST
    audio.partSize = (audio.rateHz * audio.channels) / AUDIO_PPS;   // how many bytes will be one part of samples received
}


// res = cexCallRawRead(extId, "get_samples", AUDIO_FPT, 0, AUDIO_PART_SIZE, pAudioBuffer[0]);

// if(res != STATUS_OK) {
//     (void) Cconws("get_samples failed\r\n");
// }
