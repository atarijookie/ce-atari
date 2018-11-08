#include <string.h>
#include <stdio.h>
#include "../debug.h"
#include "../utils.h"

#include "mfmcachedimage.h"

#define LOBYTE(w)   ((BYTE)(w))
#define HIBYTE(w)   ((BYTE)(((WORD)(w)>>8)&0xFF))

// crc16-ccitt generated table for fast CRC calculation
const WORD crcTable[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};

MfmCachedImage::MfmCachedImage()
{
    gotImage = false;

    // allocate tracks
    for(int i=0; i<MAX_TRACKS; i++) {
        tracks[i].mfmStream = new BYTE[MFM_STREAM_SIZE];  // allocate memory -- we're transfering 15'000 bytes, so allocate this much
    }

    // clear tracks
    clearWholeCachedImage();

    params.tracks   = 0;
    params.sides    = 0;
    params.spt      = 0;

    crc = 0;

    newContent = false;         // no new content (yet)

    // initialize encoder
    encoder.threeBits = 1;
    encoder.times = 0;
    encoder.timesCnt = 0;
}

MfmCachedImage::~MfmCachedImage()
{
    for(int i=0; i<MAX_TRACKS; i++) {
        delete []tracks[i].mfmStream;
        tracks[i].mfmStream = NULL;
    }
}

// just clear tracks
void MfmCachedImage::clearWholeCachedImage(void)    // go and memset() all the cached tracks - used on new image (not on reencode)
{
    nextIndex = 0;                      // start encoding from track 0

    for(int i=0; i<MAX_TRACKS; i++) {
        tracks[i].isReady = false;      // not ready yet
        tracks[i].bytesInStream = 0;
        memset(tracks[i].mfmStream, 0, MFM_STREAM_SIZE);
    }
}

// This method should return next track index we should encode.
// In case we need to encode specific track first (e.g. Franz wants it right now), take it from nextIndex
// If we sequentialy encode the whole image, store and restore the index to nextIndex
int MfmCachedImage::getNextIndexToEncode(void)
{
    int maxIndex;
    trackAndSideToIndex(params.tracks - 1, params.sides - 1, maxIndex); // get maximal track index, which is still allowed

    // if got valid next index and that track is not already encoded, use it
    if(nextIndex != -1 && nextIndex <= maxIndex && !tracks[nextIndex].isReady) {
        return nextIndex;
    }

    // if nextIndex not valid, go through whole image and try to find something which is not ready


    return -1;      // if wasn't able to find valid next index
}

void MfmCachedImage::encodeAndCacheImage(FloppyImage *img)
{
    if(!img->isOpen()) {                    // image file not open? quit
        return;
    }

    int tracksNo, sides, spt;
    img->getParams(tracksNo, sides, spt);   // read the floppy image params

    // store params for later usage
    params.tracks   = tracksNo;
    params.sides    = sides;
    params.spt      = spt;

    DWORD after50ms = Utils::getEndTime(50);                                // this will help to add pauses at least every 50 ms to allow other threads to do stuff

    for(int t=0; t<tracksNo; t++) {                                         // go through the whole image and encode it
        for(int s=0; s<sides; s++) {
            if(Utils::getCurrentMs() > after50ms) {                         // if at least 50 ms passed since start or previous pause, add a small pause so other threads could do stuff
                Utils::sleepMs(5);
                after50ms = Utils::getEndTime(50);
            }

            int index;
            trackAndSideToIndex(t, s, index);

            if(index == -1) {                           // index out of bounds?
                continue;
            }

            tracks[index].isReady = false;              // track not ready to be streamed - will be encoded

            memset(tracks[index].mfmStream, 0, MFM_STREAM_SIZE);    // initialize MFM stream
            bfr = tracks[index].mfmStream;                          // move pointer to start of track buffer

            encodeSingleTrack(img, s, t, spt);          // encode single track

            if(sigintReceived) {                        // app terminated? quit
                return;
            }

            int cnt = bfr - tracks[index].mfmStream;    // data count = current position - start
            tracks[index].bytesInStream = cnt;          // store the data count

            for(int i=0; i<MFM_STREAM_SIZE; i += 2) {   // swap bytes - Franz has other endiannes
                BYTE tmp                        = tracks[index].mfmStream[i + 0];
                tracks[index].mfmStream[i + 0]  = tracks[index].mfmStream[i + 1];
                tracks[index].mfmStream[i + 1]  = tmp;
            }

            tracks[index].isReady = true;               // track is now ready to be streamed
        }
    }

    //dumpTracksToFile(tracksNo, sides, spt);         // for debugging purposes - dump to file

    newContent  = true;     // we got new content!
    gotImage    = true;
}

void MfmCachedImage::trackAndSideToIndex(const int track, const int side, int &index)
{
    index = track * 2 + side;       // calculate index from track and side

    if(index >= MAX_TRACKS) {       // index out of bounds?
        index = -1;
    }
}

void MfmCachedImage::indexToTrackAndSide(const int index, int &track, int &side)
{
    if(index >= MAX_TRACKS) {       // index out of bounds?
        track = -1;
        side = -1;
        return;
    }

    side = index & 1;   // lowest bit is side
    track = index / 2;
}

void MfmCachedImage::log(char *str)
{
    FILE *f = fopen("mfmcached.log", "a+t");
    if(!f) {
        return;
    }

    fprintf(f, str);
    fclose(f);
}

void MfmCachedImage::dumpTracksToFile(int tracksNo, int sides, int spt)
{
    FILE *f = fopen("mfmcachedimage.bin", "wb");    // open file

    if(!f) {        // failed to open? quit
        return;
    }

    for(int t=0; t<tracksNo; t++) {
        for(int s=0; s<sides; s++) {
            int index;
            trackAndSideToIndex(t, s, index);

            if(index == -1) {       // index out of bounds?
                continue;
            }

            fwrite(tracks[index].mfmStream, 1, MFM_STREAM_SIZE, f);   // write to file
        }
    }

    fclose(f);
}

void MfmCachedImage::encodeSingleTrack(FloppyImage *img, int side, int track, int sectorsPerTrack)
{
    // start of the track -- we should stream 60* 0x4e
    for(int i=0; i<60; i++) {
        appendByteToStream(0x4e);
    }

    for(int sect=1; sect <= sectorsPerTrack; sect++) {
        createMfmStream(img, side, track, sect);        // then create the right MFM stream

        if(sigintReceived) {                            // app terminated? quit
            return;
        }
    }

    appendRawByte(0xF0);            // append this - this is a mark of track stream end
    appendRawByte(0x00);
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

    int index;
    trackAndSideToIndex(track, side, index);

    if(index == -1) {                                           // index out of bounds?
        bytesInBuffer = 0;
        return NULL;
    }

    bytesInBuffer = tracks[index].bytesInStream;                // return that track
    return tracks[index].mfmStream;
}

bool MfmCachedImage::createMfmStream(FloppyImage *img, int side, int track, int sector)
{
    bool res;

    BYTE data[512];
    res = img->readSector(track, side, sector, data);     // read data into 'data'

    if(!res) {
        return false;
    }

    appendCurrentSectorCommand(track, side, sector);     // append this sector mark so we would know what are we streaming out

    int i;
    for(i=0; i<12; i++) {                                   // GAP 2: 12 * 0x00
        appendByteToStream(0);
    }

    crc = 0xffff;                                           // init CRC
    for(i=0; i<3; i++) {                                    // GAP 2: 3 * A1 mark
        appendA1MarkToStream();
    }

    appendByteToStream( 0xfe );            // ID record
    appendByteToStream( track );
    appendByteToStream( side );
    appendByteToStream( sector );
    appendByteToStream( 0x02 );            // size -- 2 == 512 B per sector
    appendByteToStream( HIBYTE(crc), false);        // crc1
    appendByteToStream( LOBYTE(crc), false);        // crc2

    for(i=0; i<22; i++) {                                   // GAP 3a: 22 * 0x4e
        appendByteToStream(0x4e);
    }

    for(i=0; i<12; i++) {                                   // GAP 3b: 12 * 0x00
        appendByteToStream(0);
    }

    crc = 0xffff;                                           // init CRC
    for(i=0; i<3; i++) {                                    // GAP 3b: 3 * A1 mark
        appendA1MarkToStream();
    }

    appendByteToStream(0xfb);               // DAM mark

    for(i=0; i<512; i++) {                                  // data
        appendByteToStream(data[i]);
    }

    appendByteToStream(HIBYTE(crc), false);         // crc1
    appendByteToStream(LOBYTE(crc), false);         // crc2

    for(i=0; i<40; i++) {                                   // GAP 4: 40 * 0x4e
        appendByteToStream(0x4e);
    }

    return true;
}

//#define OLD_ENCODER

#ifdef OLD_ENCODER
// old implementation which uses appendChange()
void MfmCachedImage::appendByteToStream(BYTE val, bool doCalcCrc)
{
    if(doCalcCrc) {
        updateCrcFast(val);
    }

    static BYTE prevBit = 0;

    for(int i=0; i<8; i++) {                        // for all bits
        BYTE bit = val & 0x80;                      // get highest bit
        val = val << 1;                             // shift up

        if(bit == 0) {                              // current bit is 0?
            if(prevBit == 0) {                      // append 0 after 0?
                appendChange(1);    // R
                appendChange(0);    // N
            } else {                                // append 0 after 1?
                appendChange(0);    // N
                appendChange(0);    // N
            }
        } else {                                    // current bit is 1?
            appendChange(0);        // N
            appendChange(1);        // R
        }

        prevBit = bit;                              // store this bit for next cycle
    }
}

void MfmCachedImage::appendChange(const BYTE chg)
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

    static BYTE times       = 0;
    static BYTE timesCnt    = 0;

    times = times << 2;                 // shift 2 up
    times = times | time;               // add lowest 2 bits

    timesCnt++;                         // increment the count of times we have
    if(timesCnt == 4) {                 // we have 4 times (whole byte), store it
        timesCnt = 0;

        *bfr = times;                   // store times
        bfr++;                          // move forward in buffer
    }
}

void MfmCachedImage::appendA1MarkToStream(void)
{
    // append A1 mark in stream, which is 8-6-8-6 in MFM (normaly would been 8-6-4-4-6)
    // 8 us
    appendChange(0);  // N
    appendChange(1);  // R
    appendChange(0);  // N
    appendChange(0);  // N
    appendChange(0);  // N

    // 6 us
    appendChange(1);  // R
    appendChange(0);  // N
    appendChange(0);  // N

    // 8 us
    appendChange(1);  // R
    appendChange(0);  // N
    appendChange(0);  // N
    appendChange(0);  // N

    // 6 us
    appendChange(1);  // R
    appendChange(0);  // N
    appendChange(0);  // N
    appendChange(1);  // R

    updateCrcFast(0xa1);
}

#else

// new implementation which generates time from bit triplets (threeBits)
void MfmCachedImage::appendByteToStream(BYTE val, bool doCalcCrc)
{
    if(doCalcCrc) {
        updateCrcFast(val);
    }

    BYTE time;

    for(int i=0; i<8; i++) {                        // for all bits
        BYTE bit = (val & 0x80) >> 7;               // get highest bit (stored in lowest bit now as 0 or 1)
        encoder.threeBits = ((encoder.threeBits << 1) | bit) & 7;   // construct new 3 bits -- two old, 1 current bit (old-old-new)

        val = val << 1;                             // shift bits in input value up

        time = 0;                                   // no time yet

        switch(encoder.threeBits) {
            // threeBits with value 0,3,7 produce 4 us mfm time
            case 0:
            case 3:
            case 7: time = MFM_4US; break;

            // threeBits with value 1,4 produce 6 us mfm time
            case 1:
            case 4: time = MFM_6US; break;

            // threeBits with value 5 produce 8 us mfm time
            case 5: time = MFM_8US; break;

            // threeBits with value 2 or 6 don't produce mfm time value
        }

        if(time != 0) {     // if we found a valid time in threeBits
            encoder.times = encoder.times << 2;     // shift 2 up
            encoder.times = encoder.times | time;   // add lowest 2 bits

            encoder.timesCnt++;                     // increment the count of times we have
            if(encoder.timesCnt == 4) {             // we have 4 times (whole byte), store it
                encoder.timesCnt = 0;

                *bfr = encoder.times;               // store times
                bfr++;                              // move forward in buffer
            }
        }
    }
}

// appendTime() used in new implementation by appendA1MarkToStream(), otherwise it's integrated for speed in appendByteToStream()
void MfmCachedImage::appendTime(BYTE time)
{
    encoder.times = encoder.times << 2;     // shift 2 up
    encoder.times = encoder.times | time;   // add lowest 2 bits

    encoder.timesCnt++;                   // increment the count of times we have
    if(encoder.timesCnt == 4) {         // we have 4 times (whole byte), store it
        encoder.timesCnt = 0;

        *bfr = encoder.times;           // store times
        bfr++;                          // move forward in buffer
    }
}

void MfmCachedImage::appendA1MarkToStream(void)
{
    // the start of A1 mark depends on previous bits
    if(encoder.threeBits == 0 || encoder.threeBits == 4) {
        appendTime(MFM_6US);    // 0,4 -> 6 us
    } else if(encoder.threeBits == 2 || encoder.threeBits == 6) {
        appendTime(MFM_8US);    // 2,6 -> 8 us
    } else {
        appendTime(MFM_4US);    // 1,3,5,7 -> 4 us
    }

    // append A1 mark in stream, which is 8-6-8-6 in MFM (normaly would been 8-6-4-4-6)
    appendTime(MFM_8US);
    appendTime(MFM_6US);
    appendTime(MFM_8US);
    appendTime(MFM_6US);

    updateCrcFast(0xa1);

    encoder.threeBits = 1;      // the end of A1 is 01 in binary, so initialize threeBits to that
}
#endif

void MfmCachedImage::appendCurrentSectorCommand(int track, int side, int sector)
{
    appendRawByte(CMD_CURRENT_SECTOR);
    appendRawByte(side);
    appendRawByte(track);
    appendRawByte(sector);
}

void MfmCachedImage::appendRawByte(BYTE val)
{
    *bfr = val;     // just store this byte, no processing
    bfr++;          // move further in buffer
}

// taken from Steem Engine emulator
void MfmCachedImage::updateCrcSlow(BYTE data)
{
    for (int i=0;i<8;i++){
        crc = ((crc << 1) ^ ((((crc >> 8) ^ (data << i)) & 0x0080) ? 0x1021 : 0));
    }
}

// taken from internet (stack overflow?)
void MfmCachedImage::updateCrcFast(BYTE data)
{
    crc = crcTable[data ^ (BYTE)(crc >> (16 - 8))] ^ (crc << 8);
}

bool MfmCachedImage::getParams(int &tracks, int &sides, int &sectorsPerTrack)
{
    tracks          = params.tracks;
    sides           = params.sides;
    sectorsPerTrack = params.spt;

    return true;
}

