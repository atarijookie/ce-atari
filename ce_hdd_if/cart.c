//--------------------------------------------------
#include <mint/sysbind.h>
#include "cart.h"
#include "acsi.h"

#include "hdd_if.h"
#include "hdd_if_lowlevel.h"
#include "stdlib.h"

#define WAIT_TIMEOUT    0xff
#define WAIT_EOF        0xfe

extern volatile BYTE cart_status_byte;
extern volatile BYTE cart_success;

void cart_dma_read(BYTE *buffer, DWORD byteCount);
void cart_dma_write(BYTE *buffer, DWORD byteCount);

DWORD timeout_time;

static BYTE lastReadData;
//**************************************************************************
static BYTE wait_for_next(void)
{
    DWORD now;
    WORD val;

    while(1) {
        val = *pCartStatus;                 // read status

        if(val & STATUS_B_RPIwantsMore) {   // RPi wants more data?
            return 0;
        }

        if(val & STATUS_B_RPIisIdle) {      // RPi went idle?
            hdIf.statusByte = lastReadData; // lastReadData is the SCSI status byte
            hdIf.success = TRUE;
            *FLOCK = 0;                     // release FLOCK, so we can just quit cart_cmd() after this function
            return WAIT_EOF;
        }

        now = *HZ_200;

        if(now >= timeout_time) {           // timeout? fail
            break;
        }
    }

    *FLOCK = 0;                             // release FLOCK, so we can just quit cart_cmd() after this function
    return WAIT_TIMEOUT;                    // no interrupt and timer expired? return error
}

// --------------------------------------
void cart_cmd(BYTE ReadNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount)
{
    WORD val;
    DWORD i;
    DWORD byteCount = sectorCount << 9;             // convert sector count to byte count

    //--------
    // init result to fail codes
    hdIf.success        = FALSE;
    hdIf.statusByte     = ACSIERROR;
    hdIf.phaseChanged   = FALSE;

    //------------------
    if(hdIf.forceFlock) {                           // should force FLOCK? just set it
        *FLOCK = -1;                                // disable FDC operations
    } else {                                        // should wait before acquiring FLOCK? wait...
        // try to acquire FLOCK if possible
        timeout_time = getTicks() + TIMEOUT;        // timeout for FLOCK at this time
        WORD locked;
        while(1) {                                  // while not time out, try again
            locked = *FLOCK;                        // read current lock value

            if(!locked) {                           // if not locked, lock and continue
                *FLOCK = -1;                        // disable FDC operations
                break;
            }

            if(getTicks() >= timeout_time) {        // on time out - fail, return ACSIERROR
                return;
            }
        }
    }
    // FLOCK acquired, continue with rest

    timeout_time = getTicks() + TIMEOUT;            // timeout for whole R/W operation at this time

    //------------------
    // transfer 0th cmd byte
    val = ((WORD)cmd[0]) << 1;                      // prepare cmd byte
    val = *((volatile BYTE *)(CART_DATA + val));    // write 1st byte (0)

    //------------------
    // transfer remaining cmd bytes
    for(i=1; i<cmdLength; i++) {
        if(wait_for_next()) {                       // if next didn't come, fail
            hdIf.success = FALSE;                   // this was set wait_for_next() to TRUE, we need to fix it (don't change wait_for_next(), it's usefull at data transfers)
            return;
        }

        val = ((WORD)cmd[i]) << 1;                      // prepare cmd byte
        val = *((volatile BYTE *)(CART_DATA + val));    // write other cmd byte
    }

    //------------------
//#define CART_ASM

#ifndef CART_ASM
    // transfer data
    lastReadData = 0;

    if(ReadNotWrite) {                          // on read
        for(i=0; i<byteCount; i++) {
            if(wait_for_next()) {               // wait for next byte, and if it's timeout or idle, it was handled and we can quit
                return;
            }

            lastReadData = *pCartData;          // try to do read -- store it to lastReadData, because it might be SCSI status byte
            *buffer = lastReadData;             // store data, go further
            buffer++;
        }

        // if whole sector(s) was read, read status at the end
    } else {                                    // on write
        for(i=0; i<byteCount; i++) {
            if(wait_for_next()) {               // wait for next byte, and if it's timeout or idle, it was handled and we can quit
                return;
            }

            val = ((WORD) (*buffer)) << 1;                  // get data from buffer
            buffer++;
            lastReadData = *((volatile BYTE *)(CART_DATA + val));   // write other cmd byte, store it to lastReadData, because it might be SCSI status byte
        }

        // if whole sector(s) was read, read status at the end
    }

    //-------------------
    // transfer status byte
    if(wait_for_next()) {           // wait for next byte, and if it's timeout or idle, it was handled and we can quit
        return;
    }

    hdIf.statusByte = *pCartData;   // get it and store it
    hdIf.success = TRUE;            // success!

#else
    if(ReadNotWrite) {              // on read
        cart_dma_read(buffer, byteCount);
    } else {                        // on write
        cart_dma_write(buffer, byteCount);
    }

    hdIf.statusByte = cart_status_byte;
    hdIf.success = cart_success;
#endif

    *FLOCK = 0;                     // release FLOCK
}

//**************************************************************************
