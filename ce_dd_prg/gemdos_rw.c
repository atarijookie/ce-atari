#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>

#include <stdint.h>
#include <stdio.h>

#include "ce_dd_prg.h"
#include "xbra.h"
#include "../ce_hdd_if/hdd_if.h"
#include "../ce_hdd_if/stdlib.h"
#include "translated.h"
#include "gemdos.h"
#include "gemdos_errno.h"
#include "bios.h"
#include "main.h"

// * CosmosEx GEMDOS driver by Jookie, 2013 & 2014
// * GEMDOS hooks part (assembler and C) by MiKRO (Miro Kropacek), 2013

// ------------------------------------------------------------------
// init and hooks part - MiKRO
extern int16_t useOldGDHandler;											// 0: use new handlers, 1: use old handlers
extern int16_t useOldBiosHandler;										// 0: use new handlers, 1: use old handlers

extern int32_t (*gemdos_table[256])( void* sp );
extern int32_t (  *bios_table[256])( void* sp );

// ------------------------------------------------------------------
// CosmosEx and Gemdos part - Jookie

extern BYTE FastRAMBuffer[];

BYTE getNextDTAsFromHost(void);
DWORD copyNextDtaToAtari(void);

TFileBuffer fileBufs[MAX_FILES];

DWORD fread_small(WORD ceHandle, DWORD countNeeded, BYTE *buffer);
DWORD fread_big(WORD ceHandle, DWORD countNeeded, BYTE *buffer);

void invalidateFileBuffer(TFileBuffer *fb, WORD ceHandle, BYTE seekMode);

// ------------------------------------------------------------------
int32_t custom_fread( void *sp )
{
	int32_t res = 0;
	BYTE *params = (BYTE *) sp;

	WORD atariHandle	= (WORD)	*((WORD *) params);
	params += 2;
	DWORD count			= (DWORD)	*((DWORD *) params);
	params += 4;
	BYTE *buffer		= (BYTE *)	*((DWORD *) params);

	// check if this handle should belong to cosmosEx
	if(!handleIsFromCE(atariHandle)) {									// not called with handle belonging to CosmosEx?
		CALL_OLD_GD(Fread, atariHandle, count, buffer);
	}

	WORD ceHandle = handleAtariToCE(atariHandle);						// convert high atari handle to little CE handle

	if(!fileBufs[ceHandle].isOpen) {                                    // file not open? fail - INVALID HANDLE
        return extendByteToDword(EIHNDL);
    }

    if(fileBufs[ceHandle].bytesToEOFinvalid) {                          // if the bytes to EOF value is invalid, we need to re-read it
        getBytesToEof(ceHandle);
    }

    if(count <= 1024) {
        res = fread_small(ceHandle, count, buffer);
    } else {
        res = fread_big(ceHandle, count, buffer);
    }

    if(res > 0) {       // if the result is count of bytes read (not error code), update current stream position
        fileBufs[ceHandle].currentPos += res;
    }

    return res;
}

// for small freads get the data through the FileBuffers
DWORD fread_small(WORD ceHandle, DWORD countNeeded, BYTE *buffer)
{
    DWORD countDone = 0;
    WORD dataLeft, copyCount;

	TFileBuffer *fb = &fileBufs[ceHandle];								// to shorten the following operations use this pointer
    dataLeft        = fb->rCount - fb->rStart;	                        // see how many data we have buffered

    while(countNeeded > 0) {

        if(dataLeft == 0) {                                             // no data buffered?
            fillReadBuffer(ceHandle);									// fill the read buffer with new data
            dataLeft = fb->rCount - fb->rStart;	                        // see how many data we have buffered

            if(dataLeft == 0) {                                         // if nothing left, quit and return how many we got
                break;
            }
        }

        copyCount = (dataLeft > countNeeded) ? countNeeded : dataLeft;  // do we have more data than we need? Just use only what we need, otherwise use all data left
		memcpy(buffer, &fb->rBuf[ fb->rStart], copyCount);

        buffer      += copyCount;                                       // update pointer to where next data should be stored
        countDone   += copyCount;                                       // add to count of bytes read
        countNeeded -= copyCount;                                       // subtract from count of bytes needed to be read

        fb->rStart  += copyCount;                                       // update pointer to first unused data
        dataLeft    = fb->rCount - fb->rStart;	                        // see how many data we have buffered
    }

    fb->bytesToEOF -= countDone;                                        // update count of remaining bytes
    return countDone;
}

// for big freads use buffered data, then do big data transfer (multiple sectors at once), then finish with buffered data
DWORD fread_big(WORD ceHandle, DWORD countNeeded, BYTE *buffer)
{
	TFileBuffer *fb	    = &fileBufs[ceHandle];					        // to shorten the following operations use this pointer
	WORD dataLeft	    = fb->rCount - fb->rStart;

    DWORD countDone     = 0;
    DWORD dwBuffer      = (DWORD) buffer;
    BYTE bufferIsOdd	= dwBuffer	& 1;
	BYTE dataLeftIsOdd;
    char seekOffset     = 0;

    // First phase of BIG fread:
    // Use all the possible buffered data, and also make the buffer pointer EVEN, as the ACSI transfer works only on EVEN addresses.
    // This means that if the buffer pointer is EVEN, don't use too much data and don't make it ODD this way;
    // and if the buffer pointer is ODD, then make it EVEN - either use already available buffered data, or read data to buffer.

    if(bufferIsOdd) {                                                   // if buffer address is ODD, we need to make it EVEN before the big ACSI transfer starts
        if(dataLeft == 0) {                                             // no data buffered?
            fillReadBuffer(ceHandle);									// fill the read buffer with new data
            dataLeft = fb->rCount - fb->rStart;	                        // see how many data we have buffered

            if(dataLeft == 0) {                                         // no data in the file? fail
                fb->bytesToEOF = 0;                                     // update count of remaining bytes
                return 0;
            }
        }

        // ok, so we got some data buffered, now use only ODD number of data, so the buffer pointer after this would be EVEN
        dataLeftIsOdd = dataLeft & 1;

        if(!dataLeftIsOdd) {                                            // remaining buffered data count is EVEN? Use only ODD part
            dataLeft    -= 1;
            seekOffset  = -1;
        }
    } else {                                                            // if buffer address is EVEN, we don't need to fix the buffer to be on EVEN address
        if(dataLeft != 0) {                                             // so use buffered data if there are some (otherwise skip this step)
            dataLeftIsOdd = dataLeft & 1;

            if(dataLeftIsOdd) {                                         // if the data left is ODD, use one byte less - to keep the buffer pointer EVEN
                dataLeft--;
                seekOffset = -1;
            }
        }
    }

    if(dataLeft != 0) {                                                 // if should copy some remaining buffered data, do it
  		memcpy(buffer, &fb->rBuf[ fb->rStart], dataLeft);               // copy the data

        buffer      += dataLeft;                                        // update pointer to where next data should be stored
        countDone   += dataLeft;                                        // add to count of bytes read
        countNeeded -= dataLeft;                                        // subtract from count of bytes needed to be read

        fb->bytesToEOF -= dataLeft;                                     // update count of remaining bytes
    }

	fb->rStart = 0;													    // mark that the buffer doesn't contain any data anymore (although it might contain 1 byte)
    fb->rCount = 0;
    //---------------

    if(fb->bytesToEOF < countNeeded) {                                  // if we have less data in the file than what the caller requested, update the countNeeded
        countNeeded = fb->bytesToEOF;
    }

    // Second phase of BIG fread: transfer data by blocks of size 512 bytes, buffer must be EVEN
    BYTE  toFastRam = (((DWORD)buffer) >= 0x1000000) ? TRUE : FALSE;          // flag: are we reading to FAST RAM?
    DWORD blockSize = toFastRam ? FASTRAM_BUFFER_SIZE : (MAXSECTORS * 512); // size of block, which we will read

    DWORD res;
	while(countNeeded >= 512) {											// while we're not at the ending sector
        // To avoid corruption of data beyond the border of buffer, read LESS than what's needed - rounded to nearest lower sector count
        DWORD countNeededRoundedDown = countNeeded & 0xfffffe00;        // round to multiple of 512 (sector size)

        // If the needed count is bigger that what we can fit in maximum transfer size, limit it to that maximum; otherwise just use it.
        DWORD thisReadSizeBytes = (countNeededRoundedDown < blockSize) ? countNeededRoundedDown : blockSize;

        if(toFastRam) {     // if reading to FAST RAM, first read to fastRamBuffer, and then copy to the correct buffer
            res = readData(ceHandle, FastRAMBuffer, thisReadSizeBytes, seekOffset);
            memcpy(buffer, FastRAMBuffer, thisReadSizeBytes);
        } else {            // if reading to ST RAM, just read directly there
            res = readData(ceHandle, buffer, thisReadSizeBytes, seekOffset);
        }

		countDone	    += res;											// update the bytes read variable
		buffer		    += res;											// update the buffer pointer
		countNeeded     -= res;											// update the count that we still should read
        fb->bytesToEOF  -= res;                                         // update count of remaining bytes

		if(res != thisReadSizeBytes) {                                  // if failed to read all the requested data?
            fb->bytesToEOF = 0;                                         // update count of remaining bytes
			return countDone;										    // return with the count of read data
		}
	}
    //--------------

    // Third phase of BIG fread: if the rest after reading big blocks is not 0, we need to finish it with one last buffered read
	if(countNeeded != 0) {
		fillReadBuffer(ceHandle);

		DWORD rest = (countNeeded <= fb->rCount) ? countNeeded : fb->rCount;    // see if we have enough data to read the rest, and use which is lower - either what we want to read, or what we can read

		memcpy(buffer, &fb->rBuf[ fb->rStart ], rest);				    // copy the data that we have
		fb->rStart	+= rest;										    // and move the pointer further in buffer
		countDone	+= rest;										    // also mark that we've read this rest

        fb->bytesToEOF -= rest;                                         // update count of remaining bytes
	}

    return countDone;                                                   // return how much bytes we've read together
}

int32_t custom_fwrite( void *sp )
{
	DWORD res = 0;
	BYTE *params = (BYTE *) sp;

	WORD atariHandle	= (WORD)	*((WORD *) params);
	params += 2;
	DWORD count			= (DWORD)	*((DWORD *) params);
	params += 4;
	BYTE *buffer		= (BYTE *)	*((DWORD *) params);

	// check if this handle should belong to cosmosEx
	if(!handleIsFromCE(atariHandle)) {									// not called with handle belonging to CosmosEx?
		CALL_OLD_GD(Fwrite, atariHandle, count, buffer);
	}

	WORD ceHandle = handleAtariToCE(atariHandle);						// convert high atari handle to little CE handle

	if(!fileBufs[ceHandle].isOpen) {                                    // file not open? fail - INVALID HANDLE
        return extendByteToDword(EIHNDL);
    }

    fileBufs[ceHandle].bytesToEOFinvalid = 1;                           // mark that after this write the bytes to EOF will be invalid

	TFileBuffer *fb = &fileBufs[ceHandle];								// to shorten the following operations use this pointer
	WORD spaceLeft = (RW_BUFFER_SIZE - fb->wCount);						// how much space we have in the buffer left?

	if(count <= RW_BUFFER_SIZE) {										// if writing less than size of our buffer
		if(count <= spaceLeft) {										// the whole new data would fit in our buffer
			memcpy(&fb->wBuf[ fb->wCount ], buffer, count);				// copy in the data, update the data counter, and return the written count
			fb->wCount += count;
			fb->currentPos += count;
			return count;
		} else {														// the new data won't fit in the current (not empty) buffer
			WORD firstPart	= spaceLeft;								// store to buffer what we could store to make it full
			WORD rest		= count - firstPart;						// and calculate what will then stay after write in buffer as rest

			memcpy(&fb->wBuf[ fb->wCount ], buffer, firstPart);			// copy the 1st part to buffer, update data counter
			fb->wCount += firstPart;

			commitChanges(ceHandle);									// commit the current write buffer

			memcpy(&fb->wBuf[0], buffer + firstPart, rest);				// copy the rest to buffer, update data counter
			fb->wCount = rest;

			fb->currentPos += count;
			return count;
		}
	} else {															// if writing more than size of our buffer
		DWORD dwBuffer = (DWORD) buffer;

		BYTE  bufferIsOdd		= dwBuffer	& 1;
		BYTE  spaceLeftIsOdd	= spaceLeft	& 1;

		DWORD bytesWritten = 0;
        DWORD remCount = 0;

		// the code inside is needed only when: buffer contains some data || address is ODD! Otherwise should skip this.
		if(fb->wCount != 0 || bufferIsOdd) {

			if(!bufferIsOdd) {												// should write from EVEN address
				if(spaceLeftIsOdd) {										// and space left is odd number - make it also even
					spaceLeft--;
				}
			} else {														// should write from ODD address
				if(!spaceLeftIsOdd) {										// and space left is even number - make it also ODD, so it would cancel out the ODD shit
					if(spaceLeft > 0) {										// can we make ODD number out of it?
						spaceLeft--;
					} else {												// if can't make ODD number out of 0
						commitChanges(ceHandle);							// empty the buffer - now the spaceLeft should be 512 again

						spaceLeft = RW_BUFFER_SIZE;							// recalculate the space left variable
						spaceLeft--;										// and make ODD number out of it
					}
				}
			}

			memcpy(&fb->wBuf[ fb->wCount ], buffer, spaceLeft);				// copy some data to current buffer to use it as much as possible
			fb->wCount	+= spaceLeft;										// calculate the new data count
			buffer		+= spaceLeft;										// and calculate the new pointer to data, which should be now an EVEN number!

			res = commitChanges(ceHandle);									// empty the current buffer

			if(!res) {														// failed to write data? no data written yet
				return 0;
			}

			bytesWritten += spaceLeft;										// until now we've written this many bytes

            remCount = count - spaceLeft;                                   // this much data is remaining to be written
        } else {                                                            // if didn't go through file buffer
            remCount = count;
        }

		// ---------------------
		// at this point 'buffer' points to the remaining data and it should be an EVEN number!

		if(remCount < RW_BUFFER_SIZE) {									// if the remaining data count is less than what we should buffer
			memcpy(&fb->wBuf[ 0 ], buffer, remCount);					// copy data to current buffer
			fb->wCount += remCount;										// calculate the new data count

			fb->currentPos += count;
			return count;
		} else {														// if the remaining data count is more that we can buffer
			// transfer the remaining data in a loop
            BYTE  toFastRam = (((int)buffer) >= 0x1000000) ? TRUE : FALSE;          // flag: are we reading to FAST RAM?
            DWORD blockSize = toFastRam ? FASTRAM_BUFFER_SIZE : (MAXSECTORS * 512); // size of block, which we will read

			while(remCount > 0) {										// while there's something to send
				// calculate how much data we should transfer in this loop - with respect to MAX SECTORS we can transfer at once
                DWORD thisWriteSizeBytes = (remCount < blockSize) ? remCount : blockSize; // will the needed write size within the blockSize, or not?

                if(toFastRam) {     // if writing from FAST RAM, first cop to fastRamBuffer, and then write using DMA
                    memcpy(FastRAMBuffer, buffer, thisWriteSizeBytes);
                    res = writeData(ceHandle, FastRAMBuffer, thisWriteSizeBytes);
                } else {            // if writing from ST RAM, just writing directly there
                    res = writeData(ceHandle, buffer, thisWriteSizeBytes);
                }

				bytesWritten	+= res;									// update bytes written variable
				buffer			+= res;									// and move pointer in buffer further
				remCount		-= res;									// decrease the count that is remaining

				if(res != thisWriteSizeBytes) {                         // failed to write more data? return the count of data written
					fb->currentPos += bytesWritten;
					return bytesWritten;
				}
			}

			fb->currentPos += bytesWritten;
			return bytesWritten;										// return how much we've written total
		}
	}

	// this should never happen
	return EINTRN;
}

// BEWARE! BYTE *bfr must point to ST RAM, because DMA chip can't transfer data from/to FAST RAM!
DWORD writeData(BYTE ceHandle, BYTE *bfr, DWORD cnt)
{
	commandLong[5] = GEMDOS_Fwrite;										// store GEMDOS function number
	commandLong[6] = ceHandle;											// store file handle

    WORD sectorCount = (cnt + 511) >> 9;								// calculate how many sectors should we transfer

    commandLong[7] = cnt >> 16;											// store byte count
    commandLong[8] = cnt >>  8;
    commandLong[9] = cnt  & 0xff;

    (*hdIf.cmd)(ACSI_WRITE, commandLong, CMD_LENGTH_LONG, bfr, sectorCount);	// send command to host over ACSI

    if(!hdIf.success) {     // failed?
        return 0;
    }

    // if all data transfered, return count of all data
    if(hdIf.statusByte == RW_ALL_TRANSFERED) {
        return cnt;
    }

    // if the result is also not partial transfer, then some other error happened, return that no data was transfered
    if(hdIf.statusByte != RW_PARTIAL_TRANSFER ) {
        return 0;
    }

    // if we got here, then partial transfer happened, see how much data we got
    commandShort[4] = GD_CUSTOM_getRWdataCnt;
    commandShort[5] = ceHandle;

    (*hdIf.cmd)(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);			// send command to host over ACSI

    if(!hdIf.success || hdIf.statusByte != E_OK) {									// failed? say that no data was transfered
        return 0;
    }

    DWORD count = getDword(pDmaBuffer);						// read how much data was written
	return count;
}

// call this to fill the read buffer. Note that this destroys all the current data in buffer.
BYTE fillReadBuffer(WORD ceHandle)
{
	DWORD res;

	if(ceHandle >= MAX_FILES) {											// would be out of index? quit - with error
		return FALSE;
	}

	fileBufs[ceHandle].rCount = 0;
	fileBufs[ceHandle].rStart = 0;

	res = readData(ceHandle, fileBufs[ceHandle].rBuf, 512, 0);			// try to read 512 bytes

	fileBufs[ceHandle].rCount = res;									// store how much data we've read
	return TRUE;
}

// BEWARE! BYTE *bfr must point to ST RAM, because DMA chip can't transfer data to FAST RAM!
DWORD readData(WORD ceHandle, BYTE *bfr, DWORD cnt, BYTE seekOffset)
{
	commandLong[5] = GEMDOS_Fread;										// store GEMDOS function number
	commandLong[6] = ceHandle;											// store file handle

	commandLong[10] = seekOffset;										// seek offset before read

	WORD sectorCount = (cnt + 511) >> 9;								// calculate how many sectors should we transfer
	DWORD count=0;

    commandLong[7] = cnt >> 16;											// store byte count
    commandLong[8] = cnt >>  8;
    commandLong[9] = cnt  & 0xff;

    (*hdIf.cmd)(ACSI_READ, commandLong, CMD_LENGTH_LONG, bfr, sectorCount);	// Normal read to ST RAM - send command to host over ACSI

    if(!hdIf.success) {     // failed?
        return 0;
    }

    // if all data transfered, return count of all data
    if(hdIf.statusByte == RW_ALL_TRANSFERED) {
        return cnt;
    }

    // if the result is also not partial transfer, then some other error happened, return that no data was transfered
    if(hdIf.statusByte != RW_PARTIAL_TRANSFER ) {
        return 0;
    }

    // if we got here, then partial transfer happened, see how much data we got
    commandShort[4] = GD_CUSTOM_getRWdataCnt;
    commandShort[5] = ceHandle;

    (*hdIf.cmd)(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);				// send command to host over ACSI

    if(!hdIf.success || hdIf.statusByte != E_OK) {      // failed? say that no data was transfered
        return 0;
    }

    count = getDword(pDmaBuffer);						// read how much data was read
	return count;
}

// call this function on fclose, fseek to write the rest of write buffer to the file
BYTE commitChanges(WORD ceHandle)
{
	DWORD res;

	if(ceHandle >= MAX_FILES) {											// would be out of index? quit - with error
		return FALSE;
	}

	TFileBuffer *fb = &fileBufs[ceHandle];

	if(fb->wCount == 0) {												// nothing stored in write cache? quit - with no problem
		return TRUE;
	}

	res = writeData(ceHandle, fb->wBuf, fb->wCount);					// try to write the data

	if(res == fb->wCount) {												// if we've written all the requested data
		fb->wCount = 0;													// store that the buffer has been commited
		return TRUE;
	}

	fb->wCount = 0;														// store that the buffer has been commited
	return FALSE;
}

// call this on start to init all, or on fclose / fopen / fcreate to init it
void initFileBuffer(WORD ceHandle)
{
	if(ceHandle >= MAX_FILES) {											// would be out of index? quit
		return;
	}

    fileBufs[ceHandle].isOpen               = FALSE;                    // NOT open
    fileBufs[ceHandle].currentPos           = 0;                        // start of the file
	fileBufs[ceHandle].rCount               = 0;
	fileBufs[ceHandle].rStart               = 0;
    fileBufs[ceHandle].bytesToEOF           = 0;
    fileBufs[ceHandle].bytesToEOFinvalid    = 1;                        // mark that the bytesToEOF is invalid
	fileBufs[ceHandle].wCount               = 0;
}

void getBytesToEof(WORD ceHandle)
{
	commandShort[4] = GD_CUSTOM_getBytesToEOF;                                  // store function number
	commandShort[5] = ceHandle;

	(*hdIf.cmd)(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);      // send command to host over ACSI

    if(!hdIf.success || hdIf.statusByte != E_OK) {							    // failed? set 0 count of bytes to EOF and quit
        fileBufs[ceHandle].bytesToEOF           = 0;
        fileBufs[ceHandle].bytesToEOFinvalid    = 0;                            // mark that the bytesToEOF is valid
		return;
	}

    fileBufs[ceHandle].bytesToEOF           = getDword(pDmaBuffer);             // read and store the new count of bytes to EOF
    fileBufs[ceHandle].bytesToEOFinvalid    = 0;                                // mark that the bytesToEOF is valid
}
// ------------------------------------------------------------------
