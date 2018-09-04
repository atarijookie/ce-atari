//--------------------------------------------------
#include <mint/sysbind.h>
#include "cart.h"
#include "acsi.h"

#include "hdd_if.h"
#include "hdd_if_lowlevel.h"
#include "stdlib.h"

#define WAIT_ERROR  0xff

extern volatile BYTE cart_status_byte;
extern volatile BYTE cart_success;

void cart_dma_read(BYTE *buffer, DWORD byteCount);
void cart_dma_write(BYTE *buffer, DWORD byteCount);

//**************************************************************************
static BYTE wait_for_INT_DRQ(WORD mask)
{
    DWORD now, until;
    WORD val;

    now = *HZ_200;
    until = TIMEOUT + now;          // calc value timer must get to

    while(1) {
        val = *pCmdStatus;          // read status
        val = val & mask;           // get only wanted bits

        if (val != mask) {          // if some of the masked bits are not high, we got what we wanted, good
            return val;             // return value of bits for further evaluation
        }

        now = *HZ_200;

        if(now >= until) {          // timeout? fail
            break;
        }
    }

    return WAIT_ERROR;              // no interrupt, and timer expired, return all bits set
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
        DWORD end = getTicks() + 200;               // calculate the terminating tick count, where we should stop looking for unlocked FLOCK

        WORD locked;
        while(1) {                                  // while not time out, try again
            locked = *FLOCK;                        // read current lock value

            if(!locked) {                           // if not locked, lock and continue
                *FLOCK = -1;                        // disable FDC operations
                break;
            }

            if(getTicks() >= end) {                 // on time out - fail, return ACSIERROR
                return;
            }
        }
    }
    // FLOCK acquired, continue with rest

    //------------------
    // transfer 0th cmd byte
    val = ((WORD)cmd[0]) << 1;                      // prepare cmd byte
    val = *((volatile WORD *)(pPIOfirst + val));    // write 1st byte (0)

    //------------------
    // transfer remaining cmd bytes
    for(i=1; i<cmdLength; i++) {
        if(wait_for_INT_DRQ(STATUS_INT_DRQ) == WAIT_ERROR) {    // wait for INT, and if that didn't come, fail
            *FLOCK = 0;             // release FLOCK
            return;
        }

        val = ((WORD)cmd[i]) << 1;                      // prepare cmd byte
        val = *((volatile WORD *)(pPIOwrite + val));    // write other cmd byte
    }

    //------------------
//#define CART_ASM

#ifndef CART_ASM
    // transfer data
    if(ReadNotWrite) {                          // on read
        for(i=0; i<byteCount; i++) {
            if(wait_for_INT_DRQ(STATUS_INT_DRQ) == WAIT_ERROR) {    // wait for DRQ or INT, and if that didn't come, fail
                *FLOCK = 0;                     // release FLOCK
                return;
            }

            val = *pDMAread;                    // try to do DMA read

            if(val & STATUS2_DataIsPIOread) {   // if this wasn't data, but the last (status) byte, store it and quit
                hdIf.statusByte = val & 0xff;
                hdIf.success = TRUE;
                *FLOCK = 0;                     // release FLOCK
                return;
            }

            *buffer = val & 0xff;               // store data, go further
            buffer++;
        }

        // if whole sector(s) was read, read status at the end
    } else {                                        // on write
        for(i=0; i<byteCount; i++) {
            val = wait_for_INT_DRQ(STATUS_INT_DRQ); // wait for DRQ or INT
            if(val == WAIT_ERROR) {                 // if signal didn't come, fail
                *FLOCK = 0;                     // release FLOCK
                return;
            }

            if((val & STATUS_INT) != STATUS_INT) {    // if INT is low, quit DMA write because status byte follows
                break;
            }

            val = *buffer;                      // get data from buffer
            buffer++;
            val = *((volatile WORD *)(pDMAwrite + (val << 1)));    // try to do DMA write
        }

        // if whole sector(s) was read, read status at the end
    }

    //-------------------
    // transfer status byte
    if(wait_for_INT_DRQ(STATUS_INT) == WAIT_ERROR) {   // wait for INT, and if that didn't come, fail
        *FLOCK = 0;                 // release FLOCK
        return;
    }

    val = *pPIOread;                // get it
    hdIf.statusByte = val & 0xff;   // store it
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
