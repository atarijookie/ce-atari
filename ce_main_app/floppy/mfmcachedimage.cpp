#include <string.h>
#include <stdio.h>
#include "../debug.h"
#include "../utils.h"

#include "mfmcachedimage.h"

#define LOBYTE(w)	((BYTE)(w))
#define HIBYTE(w)	((BYTE)(((WORD)(w)>>8)&0xFF))

//#define DUMPTOFILE

#ifdef DUMPTOFILE
FILE *f;
#endif

MfmCachedImage::MfmCachedImage()
{
    gotImage = false;
    initTracks();

    #ifdef DUMPTOFILE
    f = fopen("C:\\raw_img.txt", "wt");
    #endif
	
	params.tracks	= 0;
	params.sides	= 0;
	params.spt		= 0;
    
    CRC = 0;
    
    newContent = false;         // no new content (yet)
}

MfmCachedImage::~MfmCachedImage()
{
    deleteCachedImage();

    #ifdef DUMPTOFILE
    if(f) {
        fclose(f);
    }
    #endif
}

// bufferOfBytes -- the data is transferred as WORDs, but are they stored as bytes?
// If true, swap bytes, don't append zeros. If false, no swapping, but append zeros.
void MfmCachedImage::encodeAndCacheImage(IFloppyImage *img, bool bufferOfBytes)
{
    if(gotImage) {                  // got some older image? delete it from memory
        deleteCachedImage();
    }

    if(!img->isOpen()) {            // image file not open? quit
        return;
    }

    int tracksNo, sides, spt;
    img->getParams(tracksNo, sides, spt);    // read the floppy image params

	// store params for later usage
	params.tracks	= tracksNo;
	params.sides	= sides;
	params.spt		= spt;
	
    BYTE buffer[20480];
    int bytesStored;
	
	DWORD after50ms = Utils::getEndTime(50);								// this will help to add pauses at least every 50 ms to allow other threads to do stuff

    for(int t=0; t<tracksNo; t++) {       // go through the whole image and encode it
        for(int s=0; s<sides; s++) {
			
			if(Utils::getCurrentMs() > after50ms) {							// if at least 50 ms passed since start or previous pause, add a small pause so other threads could do stuff
				Utils::sleepMs(5);
				after50ms = Utils::getEndTime(50);
			}
		
            #ifdef DUMPTOFILE
            if(f) {
                fprintf(f, "Track: %d, side: %d\n", t, s);
            }
            #endif

            encodeSingleTrack(img, s, t, spt, buffer, bytesStored, bufferOfBytes);

            int index = t * 2 + s;
            if(index >= MAX_TRACKS) {                                       // index out of bounds?
                continue;
            }

            tracks[index].bytesInStream = bytesStored;                      // store the data count
            tracks[index].mfmStream     = new BYTE[15000];                  // allocate memory -- we're transfering 15'000 bytes, so allocate this much

            memset(tracks[index].mfmStream, 0, 15000);                      // set other to 0
            memcpy(tracks[index].mfmStream, buffer, bytesStored);           // copy the memory block

            if(bufferOfBytes) {                                            // if not working on buffer of bytes, swap BYTEs in WORD
                for(int i=0; i<15000; i += 2) {
                    BYTE tmp                        = tracks[index].mfmStream[i + 0];
                    tracks[index].mfmStream[i + 0]  = tracks[index].mfmStream[i + 1];
                    tracks[index].mfmStream[i + 1]  = tmp;
                }
            }

            #ifdef DUMPTOFILE
            if(f) {
                fprintf(f, "\n\n");
            }
            #endif
        }
    }

    newContent  = true;      // we got new content!
    gotImage    = true;
}

void MfmCachedImage::encodeSingleTrack(IFloppyImage *img, int side, int track, int sectorsPerTrack, BYTE *buffer, int &bytesStored, bool bufferOfBytes)
{
    int countInSect, countInTrack=0;

    // start of the track -- we should stream 60* 0x4e
    for(int i=0; i<60; i++) {
        appendByteToStream(0x4e, buffer, countInTrack);
    }

    for(int sect=1; sect <= sectorsPerTrack; sect++) {
        if(!bufferOfBytes) {                                                            // buffer of WORDs? append to WORD
            appendZeroIfNeededToMakeEven(buffer, countInTrack);                         // this should make the sector start on even position (on full WORD, not in half)
        }

        createMfmStream(img, side, track, sect, buffer + countInTrack, countInSect);	// then create the right MFM stream
        countInTrack += countInSect;
    }

    if(!bufferOfBytes) {                                                            // buffer of WORDs? append to WORD
        appendZeroIfNeededToMakeEven(buffer, countInTrack);
    }

    appendRawByte(0xF0, buffer, countInTrack);			// append this - this is a mark of track stream end
    appendRawByte(0x00, buffer, countInTrack);

    bytesStored = countInTrack;
}

void MfmCachedImage::deleteCachedImage(void)
{
    if(!gotImage) {
        return;
    }

    for(int i=0; i<MAX_TRACKS; i++) {
        delete []tracks[i].mfmStream;

        tracks[i].bytesInStream = 0;
        tracks[i].mfmStream     = NULL;
    }

    gotImage = false;
}

BYTE *MfmCachedImage::getEncodedTrack(int track, int side, int &bytesInBuffer)
{
    if(!gotImage) {                                             // if don't have the cached stuff yet
        bytesInBuffer = 0;
        return NULL;
    }

    if(track < 0 || track > 85 || side < 0 || side > 1) {       // if track request out of bounds
        bytesInBuffer = 0;
        return NULL;
    }

    int index = track * 2 + side;
    if(index >= MAX_TRACKS) {                                   // index out of bounds?
        bytesInBuffer = 0;
        return NULL;
    }

    bytesInBuffer = tracks[index].bytesInStream;                // return that track
    return tracks[index].mfmStream;
}

void MfmCachedImage::initTracks(void)
{
    for(int i=0; i<MAX_TRACKS; i++) {
        tracks[i].bytesInStream = 0;
        tracks[i].mfmStream     = NULL;
    }
}

void MfmCachedImage::copyFromOther(MfmCachedImage &other)
{
	initTracks();
	
	DWORD after50ms = Utils::getEndTime(50);							// this will help to add pauses at least every 50 ms to allow other threads to do stuff
	
	other.getParams(params.tracks, params.sides, params.spt);			// get the params from other image
	gotImage = true;													// and mark that we got the image
	
    for(int side=0; side<2; side++) {									// copy both sides
		for(int track=0; track<params.tracks; track++) {				// copy all the tracks
		
			if(Utils::getCurrentMs() > after50ms) {						// if at least 50 ms passed since start or previous pause, add a small pause so other threads could do stuff
				Utils::sleepMs(5);
				after50ms = Utils::getEndTime(50);
			}
		
			int bytesInBuffer;
			BYTE *src = other.getEncodedTrack(track, side, bytesInBuffer);	// get pointer to source track
	
			int index = track * 2 + side;
			if(index >= MAX_TRACKS) {                                   // index out of bounds?
				continue;
			}
			
			TCachedTrack *dest = &tracks[index];						// get pointer to destination track

			if(src == NULL) {											// skip this empty SOURCE track
				if(dest->mfmStream != NULL) {
					memset(dest->mfmStream, 0, 15000);					// ....but clear it
					dest->bytesInStream = 0;
				}
				
				continue;
			}
			
			if(dest->mfmStream == NULL) {								// destination not allocated? 
				dest->mfmStream = new BYTE[15000];						// allocate memory -- we're transferring 15'000 bytes, so allocate this much
				memset(dest->mfmStream, 0, 15000);						// set other to 0
			}
			
			memcpy(dest->mfmStream, src, bytesInBuffer);				// copy data and copy the data count
			dest->bytesInStream = bytesInBuffer;
		}
    }
    
    newContent  = true;      // we got new content!
}

bool MfmCachedImage::createMfmStream(IFloppyImage *img, int side, int track, int sector, BYTE *buffer, int &count)
{
    bool res;

    count = 0;                                              // no data yet

    BYTE data[512];
    res = img->readSector(track, side, sector, data);     // read data into 'data'

    if(!res) {
        return false;
    }

    appendCurrentSectorCommand(track, side, sector, buffer, count);     // append this sector mark so we would know what are we streaming out

    int i;
    for(i=0; i<12; i++) {                                   // GAP 2: 12 * 0x00
        appendByteToStream(0, buffer, count);
    }

    CRC = 0xffff;                                           // init CRC
    for(i=0; i<3; i++) {                                    // GAP 2: 3 * A1 mark
        appendA1MarkToStream(buffer, count);
    }

    appendByteToStream( 0xfe,    buffer, count);            // ID record
    appendByteToStream( track,   buffer, count);
    appendByteToStream( side,    buffer, count);
    appendByteToStream( sector,  buffer, count);
    appendByteToStream( 0x02,    buffer, count);            // size -- 2 == 512 B per sector
    appendByteToStream( HIBYTE(CRC), buffer, count, false);        // crc1
    appendByteToStream( LOBYTE(CRC), buffer, count, false);        // crc2

    for(i=0; i<22; i++) {                                   // GAP 3a: 22 * 0x4e
        appendByteToStream(0x4e, buffer, count);
    }

    for(i=0; i<12; i++) {                                   // GAP 3b: 12 * 0x00
        appendByteToStream(0, buffer, count);
    }

    CRC = 0xffff;                                           // init CRC
    for(i=0; i<3; i++) {                                    // GAP 3b: 3 * A1 mark
        appendA1MarkToStream(buffer, count);
    }

    appendByteToStream( 0xfb, buffer, count);               // DAM mark

    for(i=0; i<512; i++) {                                  // data
        appendByteToStream( data[i], buffer, count);
    }

    appendByteToStream(HIBYTE(CRC), buffer, count, false);         // crc1
    appendByteToStream(LOBYTE(CRC), buffer, count, false);         // crc2

    for(i=0; i<40; i++) {                                   // GAP 4: 40 * 0x4e
        appendByteToStream(0x4e, buffer, count);
    }

    #ifdef DUMPTOFILE
    if(f) {
        fprintf(f, "\n\n");
    }
    #endif

    return true;
}

void MfmCachedImage::appendByteToStream(BYTE val, BYTE *bfr, int &cnt, bool doCalcCrc)
{
    #ifdef DUMPTOFILE
    if(f) {
        fprintf(f, "%02x ", val);
    }
    #endif

    if(doCalcCrc) {
        fdc_add_to_crc(CRC, val);
    }

    static BYTE prevBit = 0;

    for(int i=0; i<8; i++) {                        // for all bits
        BYTE bit = val & 0x80;                      // get highest bit
        val = val << 1;                             // shift up

        if(bit == 0) {                              // current bit is 0?
            if(prevBit == 0) {                      // append 0 after 0?
                appendChange(1, bfr, cnt);  // R
                appendChange(0, bfr, cnt);  // N
            } else {                                // append 0 after 1?
                appendChange(0, bfr, cnt);  // N
                appendChange(0, bfr, cnt);  // N
            }
        } else {                                    // current bit is 1?
            appendChange(0, bfr, cnt);              // N
            appendChange(1, bfr, cnt);              // R
        }

        prevBit = bit;                              // store this bit for next cycle
    }
}

void MfmCachedImage::appendChange(BYTE chg, BYTE *bfr, int &cnt)
{
    static BYTE changes = 0;

    changes = changes << 1;             // shift up
    changes = changes | chg;            // append change

    if(changes == 0 || changes == 1) {  // no 1 or single 1 found, quit
        return;
    }

    if(chg != 1) {                      // not adding 1 right now? quit
        return;
    }

    BYTE time = 0;

    switch(changes) {
    case 0x05:  time = MFM_4US; break;        // 4 us - stored as 1
    case 0x09:  time = MFM_6US; break;        // 6 us - stored as 2
    case 0x11:  time = MFM_8US; break;        // 8 us - stored as 3

    default:
        Debug::out(LOG_ERROR, "appendChange -- this shouldn't happen!");
        return;
    }

    changes = 0x01;                     // leave only lowest change

    appendTime(time, bfr, cnt);         // append this time to stream
}

void MfmCachedImage::appendTime(BYTE time, BYTE *bfr, int &cnt)
{
    static BYTE times       = 0;
    static BYTE timesCnt    = 0;

    times = times << 2;                 // shift 2 up
    times = times | time;               // add lowest 2 bits

    timesCnt++;                         // increment the count of times we have
    if(timesCnt == 4) {                 // we have 4 times (whole byte), store it
        timesCnt = 0;

        bfr[cnt] = times;               // store times
        cnt++;                          // increment counter of stored times
    }
}

void MfmCachedImage::appendCurrentSectorCommand(int track, int side, int sector, BYTE *buffer, int &count)
{
    appendRawByte(CMD_CURRENT_SECTOR,   buffer, count);
    appendRawByte(side,                 buffer, count);
    appendRawByte(track,                buffer, count);
    appendRawByte(sector,               buffer, count);
}

void MfmCachedImage::appendRawByte(BYTE val, BYTE *bfr, int &cnt)
{
    #ifdef DUMPTOFILE
    if(f) {
        fprintf(f, "%02x ", val);
    }
    #endif

    bfr[cnt] = val;                 // just store this byte, no processing
    cnt++;                          // increment counter of data in buffer
}

void MfmCachedImage::appendZeroIfNeededToMakeEven(BYTE *bfr, int &cnt)
{
    if((cnt & 1) != 0) {                       // odd number of bytes in the buffer? add one to make it even!
        appendRawByte(0x00, bfr, cnt);
   }
}

void MfmCachedImage::appendA1MarkToStream(BYTE *bfr, int &cnt)
{
    #ifdef DUMPTOFILE
    if(f) {
        fprintf(f, "A1 ");
    }
    #endif

    // append A1 mark in stream, which is 8-6-8-6 in MFM (normaly would been 8-6-4-4-6)
    // 8 us
    appendChange(0, bfr, cnt);  // N
    appendChange(1, bfr, cnt);  // R
    appendChange(0, bfr, cnt);  // N
    appendChange(0, bfr, cnt);  // N
    appendChange(0, bfr, cnt);  // N

    // 6 us
    appendChange(1, bfr, cnt);  // R
    appendChange(0, bfr, cnt);  // N
    appendChange(0, bfr, cnt);  // N

    // 8 us
    appendChange(1, bfr, cnt);  // R
    appendChange(0, bfr, cnt);  // N
    appendChange(0, bfr, cnt);  // N
    appendChange(0, bfr, cnt);  // N

    // 6 us
    appendChange(1, bfr, cnt);  // R
    appendChange(0, bfr, cnt);  // N
    appendChange(0, bfr, cnt);  // N
    appendChange(1, bfr, cnt);  // R

    fdc_add_to_crc(CRC, 0xa1);
}

void MfmCachedImage::fdc_add_to_crc(WORD &crc, BYTE data)
{
    for (int i=0;i<8;i++){
        crc = ((crc << 1) ^ ((((crc >> 8) ^ (data << i)) & 0x0080) ? 0x1021 : 0));
    }
}

bool MfmCachedImage::getParams(int &tracks, int &sides, int &sectorsPerTrack)
{
    tracks          = params.tracks;
    sides           = params.sides;
    sectorsPerTrack = params.spt;

    return true;
}

