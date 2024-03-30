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
#include "main.h"
#include "stream.h"
#include "playback.h"
#include "video.h"
#include "audio.h"

uint8_t buffer[BUFFER_SIZE];
uint8_t *pBuffer;

TMachine machine;
uint8_t extId;

void getScreenRateFromSyncMode(void)
{
    uint8_t syncMode = *REG_VIDEO_SHIFTER_SYNC_MODE;
    machine.screenRateHz = (syncMode & 2) ? 60 : 50;    // bit 1 set means 60 Hz, otherwise 50 Hz
}

// get current palette type, screen resolution, screen rate
void getMachineDetails(void)
{
    Supexec(getMachineType);

    switch(machine.type) {
        case MACHINE_ST:    machine.paletteType = VID_PALETTE_ST; break;
        case MACHINE_STE:   machine.paletteType = VID_PALETTE_STE; break;
        default:            machine.paletteType = VID_PALETTE_NONE; break;
    }

    uint16_t res = Getrez();

    switch(res) {
        case 0: machine.resolution = VID_RES_ST_LOW; break;
        case 1: machine.resolution = VID_RES_ST_MID; break;
        case 2: machine.resolution = VID_RES_ST_HIGH; break;
        default: machine.resolution = VID_RES_OFF; break;
    }

    if(machine.resolution == VID_RES_ST_HIGH) { // ST high runs on 70 Hz
        machine.screenRateHz = 70;
    } else {    // for ST mid and ST low find out the frequency
        Supexec(getScreenRateFromSyncMode);
    }
}

//--------------------------------------------------
int main(void)
{
    pBuffer = addrToEven(buffer);

    Clear_home();
    getMachineDetails();    // fill vars about this machine which we need for video playback

    if(machine.type != MACHINE_ST && machine.type != MACHINE_STE) {
        showMessage("Only ST and STE are supported.\r\nTerminating. (press any key)\r\n", 3);
        return 0;
    }

    //------
    // Before calling any function from the extension, you first must successfully open the extension first.
    // It may take several seconds / minutes, because the extension might be downloaded from internet,
    // it might be compiled or other preparations might be done on first run, and they it takes a little time
    // for it to start. Make sure you either specify long enough timeout time, or try to multiple times.
    (void) Cconws("Opening extension\r\n");
    extId = cexOpen("ext_vidaud", "", 5);        // open extension

    if(extId > ANY_ERROR) {     // on error, fail
        showMessage("Opening extension failed (press any key)\r\n", 3);
        return 0;
    }

    //------
    audioInitParams();  // init audio params - need to do this for the audio.* values to be set

    uint8_t res = cexCallLong(extId, "start", 6, VIDEO_FPS, machine.resolution, machine.paletteType, audio.rateHz, audio.channels, "/home/jookie/bad_apple.mp4");

    if(res != STATUS_OK) {      // calling function failed?
        showMessage("start - call failed\r\n", 3);
        return 0;
    }

    playback();

    //------
    cexCallRawWrite(extId, "stop", 0, 0, 0, pBuffer);   // stop the stream if still running
    cexClose(extId);                                    // close extension

    showMessage("Terminatimg (press any key)\r\n", 3);
    return 0;
}
