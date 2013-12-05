#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <unistd.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "xbra.h"
#include "acsi.h"
#include "translated.h"
#include "gemdos.h"
#include "gemdos_errno.h"
#include "bios.h"
#include "main.h"

// * CosmosEx GEMDOS driver by Jookie, 2013
// * GEMDOS hooks part (assembler and C) by MiKRO (Miro Kropacek), 2013
 
// ------------------------------------------------------------------ 
// init and hooks part - MiKRO 
extern int16_t useOldGDHandler;											// 0: use new handlers, 1: use old handlers 
extern int16_t useOldBiosHandler;										// 0: use new handlers, 1: use old handlers  

extern int32_t (*gemdos_table[256])( void* sp );
extern int32_t (  *bios_table[256])( void* sp );

// ------------------------------------------------------------------ 
// CosmosEx and Gemdos part - Jookie 

extern BYTE dmaBuffer[DMA_BUFFER_SIZE + 2];
extern BYTE *pDmaBuffer;

extern BYTE deviceID;
extern BYTE commandShort[CMD_LENGTH_SHORT];
extern BYTE commandLong[CMD_LENGTH_LONG];

extern BYTE *pDta;
extern BYTE tempDta[45];

extern WORD dtaCurrent, dtaTotal;
extern BYTE dtaBuffer[DTA_BUFFER_SIZE + 2];
extern BYTE *pDtaBuffer;
extern BYTE fsnextIsForUs, tryToGetMoreDTAs;

BYTE getNextDTAsFromHost(void);
DWORD copyNextDtaToAtari(void);

extern WORD ceDrives;
extern BYTE currentDrive;

#define RW_BUFFER_SIZE		512

typedef struct 
{
	WORD rCount;					// how much data is buffer (specifies where the next read data could go)
	WORD rStart;					// starting index of where we should start reading the buffer
	BYTE rBuf[RW_BUFFER_SIZE];
	
	WORD wCount;					// how much data we have in this buffer
	BYTE wBuf[RW_BUFFER_SIZE];
	
} TFileBuffer;

TFileBuffer fileBufs[MAX_FILES];

// ------------------------------------------------------------------ 
int32_t custom_fread( void *sp )
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
		CALL_OLD_GD(Fread, atariHandle, count, buffer);
	}
	
	WORD ceHandle = handleAtariToCE(atariHandle);						// convert high atari handle to little CE handle 
	
	TFileBuffer *fb	= &fileBufs[ceHandle];								// to shorten the following operations use this pointer 
	WORD dataLeft	= fb->rCount - fb->rStart;										

	if(count <= RW_BUFFER_SIZE) {										// if reading less than buffer size 
		if(count <= dataLeft) {											// if we want to read less (or equal) data than we have in buffer 
			memcpy(buffer, &fb->rBuf[ fb->rStart], count);				// copy the data to buffer and quit with success 
			fb->rStart += count;										// and more the pointer further in buffer 
			return count;
		} else {														// if we want to read more data than we have in buffer 
			memcpy(buffer, &fb->rBuf[ fb->rStart], dataLeft);			// copy all the remaining data to buffer
			buffer += dataLeft;											// update the buffer pointer to place where the remaining data should be stored
		
			fillReadBuffer(ceHandle);									// fill the read buffer with new data
			
			WORD rest	= count - dataLeft;								// calculate the rest that we should copy
			dataLeft	= fb->rCount - fb->rStart;						// and recaulculate dataLeft variable
			
			if(dataLeft >= rest) {										// we have enough data to copy 
				memcpy(buffer, &fb->rBuf[ fb->rStart ], rest);			// copy the rest of requested data
				fb->rStart += rest;										// and move the pointer further in buffer 
				return count;
			} else {													// we don't have enough data to copy :(
				WORD countMissing = rest - dataLeft;					// calculate how much data we miss
			
				memcpy(buffer, &fb->rBuf[ fb->rStart ], dataLeft);		// copy the data that we have
				fb->rStart += dataLeft;									// and move the pointer further in buffer 
				return (count - countMissing);
			}			
		}	
	} else {															// if reading more than buffer size 
		DWORD dwBuffer = (DWORD) buffer;
		
		BYTE  bufferIsOdd	= dwBuffer	& 1;
		BYTE  dataLeftIsOdd	= dataLeft	& 1;
		
		DWORD	bytesRead = 0;
		WORD	useCountFromBuffer = dataLeft;
		char	seekOffset = 0;	
	
		// the following code tries to use as much of the data we have in buffer as possible (either dataLeft, or dataLeft-1 count), making the final buffer an EVEN pointer
		if(bufferIsOdd) {												// if buffer is ODD
			if(!dataLeftIsOdd) {										// and data left is EVEN
				if(dataLeft > 0) {										// got some data left?
					useCountFromBuffer--;								// use only ODD part, and seek -1 before read
					seekOffset = -1;
				} else {												// no data left?
					fillReadBuffer(ceHandle);							// fill the read buffer with new data
					
					dataLeft = fb->rCount - fb->rStart;					// recalculate how much data we got
					
					if(dataLeft == 0) {									// no more data left? quit
						return 0;
					}
					
					dataLeftIsOdd		= dataLeft	& 1;
					useCountFromBuffer	= dataLeft;
					
					if(!dataLeftIsOdd) {								// data left is EVEN
						useCountFromBuffer--;							// use only ODD part, and seek -1 before read
						seekOffset = -1;
					}
				}
			}
		} else {														// if buffer is EVEN
			if(dataLeftIsOdd) {											// the data left are ODD
				useCountFromBuffer--;									// use only EVEN part, and seek -1 before read
				seekOffset = -1;
			} 
		}
	
		// at this point we should have: ODD buffer pointer and ODD data count, or EVEN buffer pointer and EVEN data count
		memcpy(buffer, &fb->rBuf[ fb->rStart ], useCountFromBuffer);	// copy the data that we have
		buffer		+= useCountFromBuffer;								// update the buffer pointer
		bytesRead	+= useCountFromBuffer;								// update the count of bytes we've read
		count		-= useCountFromBuffer;								// and update how much we have to read
		
		fb->rStart = 0;													// mark that the buffer doesn't contain any data anymore (although it might contain 1 byte)
		fb->rCount = 0;

		// buffer is now EVEN pointer, and the read operation must not read the last sector in the original buffer (to avoid buffer overflow)

		// do big transfers now - up to MAXSECTORS size of transfers
		while(count >= 512) {											// while we're not at the ending sector
			WORD sectorCount = count / 512; 
		
			if(sectorCount > MAXSECTORS) {								// limit the maximum sectors read in one cycle to MAXSECTORS
				sectorCount = MAXSECTORS;
			}
			
			DWORD bytesCount = ((DWORD) sectorCount) * 512;				// convert sector count to byte count
			
			res = readData(ceHandle, buffer, bytesCount, seekOffset);	// try to read the data
			
			bytesRead	+= res;											// update the bytes read variable
			buffer		+= res;											// update the buffer pointer
			count		-= res;											// update the count that we still should read
			
			if(res != bytesCount) {										// if failed to read all the requested data?
				return bytesRead;										// return with the count of read data 
			}
		}
		
		// if we should still read something little at the end
		if(count != 0) {												
			fillReadBuffer(ceHandle);
			
			DWORD rest = (count <= fb->rCount) ? count : fb->rCount;	// see if we have enough data to read the rest, and use which is lower - either what we want to read, or what we can read
			
			memcpy(buffer, &fb->rBuf[ fb->rStart ], rest);				// copy the data that we have
			fb->rStart	+= rest;										// and move the pointer further in buffer
			bytesRead	+= rest;										// also mark that we've read this rest 
		}

		return bytesRead;												// return the total count or bytes read
	}
	
	// this should never happen 
	return EINTRN;
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
	
	TFileBuffer *fb = &fileBufs[ceHandle];								// to shorten the following operations use this pointer 
	WORD spaceLeft = (RW_BUFFER_SIZE - fb->wCount);						// how much space we have in the buffer left? 
	
	if(count <= RW_BUFFER_SIZE) {										// if writing less than size of our buffer 
		if(count <= spaceLeft) {										// the whole new data would fit in our buffer 
			memcpy(&fb->wBuf[ fb->wCount ], buffer, count);				// copy in the data, update the data counter, and return the written count 
			fb->wCount += count;
			return count;
		} else {														// the new data won't fit in the current (not empty) buffer 
			WORD firstPart	= spaceLeft;								// store to buffer what we could store to make it full 
			WORD rest		= count - firstPart;						// and calculate what will then stay after write in buffer as rest 

			memcpy(&fb->wBuf[ fb->wCount ], buffer, firstPart);			// copy the 1st part to buffer, update data counter 
			fb->wCount += firstPart;
			
			commitChanges(ceHandle);									// commit the current write buffer 
			
			memcpy(&fb->wBuf[0], buffer + firstPart, rest);				// copy the rest to buffer, update data counter 
			fb->wCount = rest;
			
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

			return count;												
		} else {														// if the remaining data count is more that we can buffer 
			// transfer the remaining data in a loop 
			while(remCount > 0) {										// while there's something to send 
				// calculate how much data we should transfer in this loop - with respect to MAX SECTORS we can transfer at once 
				DWORD dataNow = (remCount <= (MAXSECTORS*512)) ? remCount : (MAXSECTORS*512);
			
				res = writeData(ceHandle, buffer, dataNow);				// send the data 

				bytesWritten	+= res;									// update bytes written variable 
				buffer			+= res;									// and move pointer in buffer further 
				remCount		-= res;									// decrease the count that is remaining 

				if(res != dataNow) {									// failed to write more data? return the count of data written 
					return bytesWritten;
				}
			}		
			
			return bytesWritten;										// return how much we've written total 
		}
	}
	
	// this should never happen 
	return EINTRN;
}

DWORD writeData(BYTE ceHandle, BYTE *bfr, DWORD cnt)
{
	commandLong[5] = GEMDOS_Fwrite;										// store GEMDOS function number 
	commandLong[6] = ceHandle;											// store file handle 
	
	commandLong[7] = cnt >> 16;											// store byte count 
	commandLong[8] = cnt >>  8;
	commandLong[9] = cnt  & 0xff;
	
	WORD sectorCount = cnt / 512;										// calculate how many sectors should we transfer 
	
	if((cnt % 512) != 0) {												// and if we have more than full sector(s) in buffer, send one more! 
		sectorCount++;
	}
	
	BYTE res = acsi_cmd(ACSI_WRITE, commandLong, CMD_LENGTH_LONG, bfr, sectorCount);	// send command to host over ACSI 
	
	// if all data transfered, return count of all data
	if(res == RW_ALL_TRANSFERED) {
		return cnt;
	}

	// if the result is also not partial transfer, then some other error happened, return that no data was transfered
	if(res != RW_PARTIAL_TRANSFER ) {
		return 0;	
	}
	
	// if we got here, then partial transfer happened, see how much data we got
	commandShort[4] = GD_CUSTOM_getRWdataCnt;
	commandShort[5] = ceHandle;										
	
	res = acsi_cmd(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);			// send command to host over ACSI

	if(res != E_OK) {													// failed? say that no data was transfered
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

DWORD readData(WORD ceHandle, BYTE *bfr, DWORD cnt, BYTE seekOffset)
{
	commandLong[5] = GEMDOS_Fread;										// store GEMDOS function number 
	commandLong[6] = ceHandle;											// store file handle 
	
	commandLong[7] = cnt >> 16;											// store byte count 
	commandLong[8] = cnt >>  8;
	commandLong[9] = cnt  & 0xff;
	
	commandLong[10] = seekOffset;										// seek offset before read
	
	WORD sectorCount = cnt / 512;										// calculate how many sectors should we transfer 
	
	if((cnt % 512) != 0) {												// and if we have more than full sector(s) in buffer, send one more! 
		sectorCount++;
	}
	
	BYTE res = acsi_cmd(ACSI_READ, commandLong, CMD_LENGTH_LONG, bfr, sectorCount);	// send command to host over ACSI 
	
	// if all data transfered, return count of all data
	if(res == RW_ALL_TRANSFERED) {
		return cnt;
	}

	// if the result is also not partial transfer, then some other error happened, return that no data was transfered
	if(res != RW_PARTIAL_TRANSFER ) {
		return 0;	
	}
	
	// if we got here, then partial transfer happened, see how much data we got
	commandShort[4] = GD_CUSTOM_getRWdataCnt;
	commandShort[5] = ceHandle;										
	
	res = acsi_cmd(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);				// send command to host over ACSI

	if(res != E_OK) {													// failed? say that no data was transfered
		return 0;
	}

    DWORD count = getDword(pDmaBuffer);						// read how much data was read
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

// call this on fseek - this will either alter the pointer in current file buffer, or invalidate the whole file buffer
void seekInFileBuffer(WORD ceHandle, int32_t offset, BYTE seekMode) 
{
	if(ceHandle >= MAX_FILES) {											// would be out of index? quit
		return;
	}

	TFileBuffer *fb = &fileBufs[ceHandle];
	
	if(seekMode != 1) {													// if doing anything other than SEEK_CUR, just invalidate the buffer
		fb->rCount = 0;
		fb->rStart = 0;
		return;
	}
	
	int32_t newPos = ((int32_t) fb->rStart) + offset;					// calculate the new position
	
	if(newPos < 0 || newPos > fb->rCount) {								// the seek would go outsite of our buffer? invalidate it!
		fb->rCount = 0;
		fb->rStart = 0;
		return;
	}
	
	fb->rStart = newPos;												// store the new position in the buffer
}

// call this on start to init all, or on fclose / fopen / fcreate to init it 
void initFileBuffer(WORD ceHandle) 
{
	if(ceHandle >= MAX_FILES) {											// would be out of index? quit 
		return;
	}

	fileBufs[ceHandle].rCount = 0;
	fileBufs[ceHandle].rStart = 0;
	fileBufs[ceHandle].wCount = 0;
}

// ------------------------------------------------------------------ 

