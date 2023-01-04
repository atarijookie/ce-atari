#include <string.h>
#include <stdio.h>
#include "../debug.h"
#include "../utils.h"
#include "../global.h"

#include "mfmcachedimage.h"

#define LOBYTE(w)   ((uint8_t)(w))
#define HIBYTE(w)   ((uint8_t)(((uint16_t)(w)>>8)&0xFF))

extern pthread_mutex_t floppyEncoderMutex;
extern THwConfig    hwConfig;

#define DAM_MARK    0xfb
#define ID_MARK     0xfe

// crc16-ccitt generated table for fast CRC calculation
const uint16_t crcTable[256] = {
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
        tracks[i].mfmStream = new uint8_t[MFM_STREAM_SIZE];  // allocate memory -- we're transfering 15'000 bytes, so allocate this much
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
    gotImage = false;
    maxTotalSymbolsTime = 0;

    for(int i=0; i<MAX_TRACKS; i++) {
        tracks[i].isReady = false;      // not ready yet
        tracks[i].bytesInStream = 0;
        tracks[i].symbolsInStream = 0;
        tracks[i].totalSymbolsTime = 0;
        memset(tracks[i].mfmStream, 0, MFM_STREAM_SIZE);
    }
}

// call this after opening the FloppyImage, before encoding
void MfmCachedImage::storeImageParams(FloppyImage *img)
{
    // clear the params first
    params.tracks = 0;
    params.sides = 0;
    params.spt = 0;

    if(!img->isLoaded()) {    // image file not open? quit
        return;
    }

    // read the floppy image params
    img->getParams(params.tracks, params.sides, params.spt);

    // calculate how many tracks we need to process
    tracksToBeEncoded = params.tracks * params.sides;

    gotImage = true;            // we got the image, we just need to encode it
    newContent = true;          // we got new content!
}

void MfmCachedImage::askToReencodeTrack(int track, int side)
{
    int index;
    trackAndSideToIndex(track, side, index);    // try to get index from track and side

    if(index == -1) {               // failed to get index?
        return;
    }

    tracks[index].encodeRequestTime = Utils::getCurrentMs();    // mark time when this request was placed
    tracks[index].isReady = false;  // the reencoding didn't happen yet
    nextIndex = index;              // store this as next index that needs reencoding
    tracksToBeEncoded++;            // increment count of tracks that need reencoding
}

bool MfmCachedImage::somethingToBeEncoded(void)
{
    return(tracksToBeEncoded > 0);  // if count of tracks to be encoded is not zero, we still need to run encoding on this image
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
    for(int i=0; i <= maxIndex; i++) {
        if(!tracks[i].isReady) {            // this track is not ready? return index
            return i;
        }
    }

    // if wasn't able to find valid next index
    return -1;
}

bool MfmCachedImage::findNotReadyTrackAndEncodeIt(FloppyImage *img, int &track, int &side)
{
    track = -1;                         // nothing encoded
    side = -1;

    if(!img->isLoaded()) {              // image file not open? quit
        return false;
    }

    int index = getNextIndexToEncode(); // get next index for encoding, or -1 if there's nothing to encode

    if(index == -1) {                   // nothing to do? quit
        tracksToBeEncoded = 0;          // nothing more to encode
        nextIndex = -1;                 // store that we don't expect anything to be encoded next
        return false;
    }

    nextIndex = index + 1;              // encoding track at index, encoding nextIndex in next loop

    int t, s;
    indexToTrackAndSide(index, t, s);

    if(t == -1) {                       // track out of bounds?
        return false;
    }

    track = t;                          // store track+side for caller
    side = s;

    //-----
    pthread_mutex_lock(&floppyEncoderMutex);      // unlock the mutex

    tracks[index].encodeActionTime = Utils::getCurrentMs(); // mark time when we started to process this track
    tracks[index].isReady = false;      // track not ready to be streamed - will be encoded

    pthread_mutex_unlock(&floppyEncoderMutex);      // unlock the mutex
    //-----

    memset(tracks[index].mfmStream, 0, MFM_STREAM_SIZE);    // initialize MFM stream
    currentStreamStart = tracks[index].mfmStream;   // where the stream starts

    #define STREAM_TABLE_ITEMS  20
    #define STREAM_TABLE_SIZE   (2 * STREAM_TABLE_ITEMS)

    #define STREAM_TABLE_OFFSET 10              // the stream table in Franz starts at this offset, because first 5 words are empty (ATN + sizes + other)
    #define STREAM_START_OFFSET (STREAM_TABLE_OFFSET + STREAM_TABLE_SIZE)

    bfr = currentStreamStart + STREAM_TABLE_SIZE;   // where the MFM data will start (after initial table)
    bytesInBfr = STREAM_TABLE_SIZE;             // no bytes in stream yet

    for(int i=0; i<STREAM_TABLE_ITEMS; i++) {   // init the table for all sectors to start (if sector missing, will restart stream)
        setRawWordAtIndex(i, STREAM_START_OFFSET);
    }

    // init encoder counters of symbols
    encoder.symbolsInStream = 0;
    encoder.totalSymbolsTime = 0;

    // init track @ index counters of symbols
    tracks[index].symbolsInStream = 0;
    tracks[index].totalSymbolsTime = 0;

    encodeSingleTrack(img, s, t, params.spt);   // encode single track

    // copy the counters from encoder to this track @ index
    tracks[index].symbolsInStream = encoder.symbolsInStream;
    tracks[index].totalSymbolsTime = encoder.totalSymbolsTime;

    // found new longest track?
    if(maxTotalSymbolsTime < tracks[index].totalSymbolsTime) {
        maxTotalSymbolsTime = tracks[index].totalSymbolsTime;
        float avgTimePerSymbol = ((float) tracks[index].totalSymbolsTime) / ((float) tracks[index].symbolsInStream);
        Debug::out(LOG_DEBUG, "MfmCachedImage::findNotReadyTrackAndEncodeIt() - new longest track has %d ms total symbols time, has %d symbols in it, the average symbol time is %.2f us", tracks[index].totalSymbolsTime / 1000, tracks[index].symbolsInStream, avgTimePerSymbol);
    }

    tracks[index].bytesInStream = bytesInBfr;   // store the data count
    setRawWordAtIndex(0, STREAM_TABLE_OFFSET + bytesInBfr);     // stream table - index 0: stream size in bytes (include those extra 5 empty WORDs on start in Franz)

    if(hwConfig.version == 1 || hwConfig.version == 2) {    // HW v1 and v2 need byte swap, HW v3 needs bytes in the original order
        for(int i=0; i<MFM_STREAM_SIZE; i += 2) {           // swap bytes - Franz has other endiannes
            uint8_t tmp                        = tracks[index].mfmStream[i + 0];
            tracks[index].mfmStream[i + 0]  = tracks[index].mfmStream[i + 1];
            tracks[index].mfmStream[i + 1]  = tmp;
        }
    }

    //-----
    pthread_mutex_lock(&floppyEncoderMutex);      // unlock the mutex

    if(tracks[index].encodeRequestTime <= tracks[index].encodeActionTime) {  // if there wasn't any request since we started to encode this track, it's ready (otherwise needs reencoding)
        tracks[index].isReady = true;               // track is now ready to be streamed
    }

    pthread_mutex_unlock(&floppyEncoderMutex);      // unlock the mutex
    //-----

    return true;
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
    std::string logFilePath = Utils::dotEnvValue("LOG_DIR", "/var/log/ce");     // path to logs dir
    Utils::mergeHostPaths(logFilePath, "mfmcached.log");             // full path = dir + filename

    FILE *f = fopen(logFilePath.c_str(), "a+t");
    if(!f) {
        return;
    }

    fputs(str, f);
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
        setRawWordAtIndex(sect, bytesInBfr);            // stream table - index #sector: offset to this sector

        encodeSingleSector(img, side, track, sect);     // then create the right MFM stream

        if(sigintReceived) {                            // app terminated? quit
            return;
        }
    }

    appendRawByte(0xF0);            // append this - this is a mark of track stream end
    appendRawByte(0x00);
}

bool MfmCachedImage::encodedTrackIsReady(int track, int side)
{
    if(!gotImage || track < 0 || track > 85 || side < 0 || side > 1) {  // invalid args?
        //Debug::out(LOG_DEBUG, "MfmCachedImage::encodedTrackIsReady FALSE -- gotImage: %d, track: %d, side: %d", gotImage, track, side);
        return false;   // not ready
    }

    int index;
    trackAndSideToIndex(track, side, index);

    if(index == -1) {   // index out of bounds?
        //Debug::out(LOG_DEBUG, "MfmCachedImage::encodedTrackIsReady FALSE -- index == 1");
        return false;   // not ready
    }

    //Debug::out(LOG_DEBUG, "MfmCachedImage::encodedTrackIsReady [%d] = %d", index, tracks[index].isReady);
    return tracks[index].isReady;   // return if ready
}

uint8_t *MfmCachedImage::getEncodedTrack(int track, int side, int &bytesInBuffer)
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

bool MfmCachedImage::encodeSingleSector(FloppyImage *img, int side, int track, int sector)
{
    bool res;

    uint8_t data[512];
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

    appendByteToStream( ID_MARK );            // ID record
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

    appendByteToStream(DAM_MARK);               // DAM mark

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

// new implementation which generates time from bit triplets (threeBits)
void MfmCachedImage::appendByteToStream(uint8_t val, bool doCalcCrc)
{
    if(doCalcCrc) {
        updateCrcFast(val);
    }

    uint8_t time;

    for(int i=0; i<8; i++) {                        // for all bits
        uint8_t bit = (val & 0x80) >> 7;               // get highest bit (stored in lowest bit now as 0 or 1)
        encoder.threeBits = ((encoder.threeBits << 1) | bit) & 7;   // construct new 3 bits -- two old, 1 current bit (old-old-new)

        val = val << 1;                             // shift bits in input value up

        time = 0;                                   // no time yet

        switch(encoder.threeBits) {
            // threeBits with value 0,3,7 produce 4 us mfm time
            case 0:
            case 3:
            case 7: time = MFM_4US;
                    encoder.symbolsInStream++;
                    encoder.totalSymbolsTime += 4;
                    break;

            // threeBits with value 1,4 produce 6 us mfm time
            case 1:
            case 4: time = MFM_6US;
                    encoder.symbolsInStream++;
                    encoder.totalSymbolsTime += 6;
                    break;

            // threeBits with value 5 produce 8 us mfm time
            case 5: time = MFM_8US;
                    encoder.symbolsInStream++;
                    encoder.totalSymbolsTime += 8;
                    break;

            // threeBits with value 2 or 6 don't produce mfm time value
        }

        if(time != 0) {     // if we found a valid time in threeBits
            encoder.times = encoder.times << 2;     // shift 2 up
            encoder.times = encoder.times | time;   // add lowest 2 bits

            encoder.timesCnt++;                     // increment the count of times we have
            if(encoder.timesCnt == 4) {             // we have 4 times (whole byte), store it
                encoder.timesCnt = 0;

                if(bytesInBfr < MFM_STREAM_SIZE) {  // still have some space in buffer? store value
                    bytesInBfr++;

                    *bfr = encoder.times;           // store times
                    bfr++;                          // move forward in buffer
                }
            }
        }
    }
}

// appendTime() used in new implementation by appendA1MarkToStream(), otherwise it's integrated for speed in appendByteToStream()
void MfmCachedImage::appendTime(uint8_t time)
{
    encoder.times = encoder.times << 2;     // shift 2 up
    encoder.times = encoder.times | time;   // add lowest 2 bits

    encoder.timesCnt++;                   // increment the count of times we have
    if(encoder.timesCnt == 4) {         // we have 4 times (whole byte), store it
        encoder.timesCnt = 0;

        if(bytesInBfr < MFM_STREAM_SIZE) {  // still have some space in buffer? store value
            bytesInBfr++;

            *bfr = encoder.times;           // store times
            bfr++;                          // move forward in buffer
        }
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

void MfmCachedImage::appendCurrentSectorCommand(int track, int side, int sector)
{
    appendRawByte(CMD_CURRENT_SECTOR);
    appendRawByte(side);
    appendRawByte(track);
    appendRawByte(sector);
}

void MfmCachedImage::setRawWordAtIndex(int index, uint16_t val)
{
    uint16_t *pWord = (uint16_t *) currentStreamStart;  // get word pointer to start of current stream
    pWord += index;                             // move pointer to the wanted offset (at index)
    *pWord = val;                               // store the value
}

void MfmCachedImage::appendRawByte(uint8_t val)
{
    if(bytesInBfr < MFM_STREAM_SIZE) {  // still have some space in buffer? store value
        bytesInBfr++;

        *bfr = val;     // just store this byte, no processing
        bfr++;          // move further in buffer
    }
}

// taken from Steem Engine emulator
void MfmCachedImage::updateCrcSlow(uint8_t data)
{
    for (int i=0;i<8;i++){
        crc = ((crc << 1) ^ ((((crc >> 8) ^ (data << i)) & 0x0080) ? 0x1021 : 0));
    }
}

// taken from internet (stack overflow?)
void MfmCachedImage::updateCrcFast(uint8_t data)
{
    crc = crcTable[data ^ (uint8_t)(crc >> (16 - 8))] ^ (crc << 8);
}

bool MfmCachedImage::getParams(int &tracks, int &sides, int &sectorsPerTrack)
{
    tracks          = params.tracks;
    sides           = params.sides;
    sectorsPerTrack = params.spt;

    return true;
}

uint8_t MfmCachedImage::getMfmTime(void)
{
    if(decoder.usedTimesFromByte >= 4) {    // if used all 4 MFM times from byte
        decoder.usedTimesFromByte = 0;      // reset uset times counter
        decoder.pBfr++;                     // move to next byte
        decoder.count--;                    // decrement data count in buffer

        if(decoder.count == 0) {            // nothing more to process? we're done
            decoder.done = true;
        }
    }

    uint8_t val = ((*decoder.pBfr) >> 6) & 0x03;   // get highest 2 bits
    *decoder.pBfr = (*decoder.pBfr) << 2;       // shift other bits up
    decoder.usedTimesFromByte++;                // increment how many times we used

    return val;                                 // return highest 2 bits
}

void MfmCachedImage::addOneBit(uint8_t bit, bool newRemainder)
{
    decoder.dByte = (decoder.dByte << 1) | bit;
    decoder.bCount++;
    decoder.remainder = newRemainder;
}

void MfmCachedImage::addTwoBits(uint8_t bits, bool newRemainder)
{
    if(decoder.bCount < 7) {    // if we got 6 or less bits, we can add both bits immediatelly
        decoder.dByte = (decoder.dByte << 2) | bits;
        decoder.bCount += 2;
    } else {                    // if we got 7 bits, we need to add then individually
        decoder.dByte = (decoder.dByte << 1) | (bits >> 1); // add upper bit
        decoder.bCount++;       // now we got 8 bits

        handleDecodedByte();    // store byte where it should go

        decoder.dByte = (bits & 1);                         // add lower bit
        decoder.bCount = 1;     // now we got 1 bit
    }

    decoder.remainder = newRemainder;
}

void MfmCachedImage::handleDecodedByte(void)
{
    decoder.bCount = 0;

    if(decoder.byteOffset == 0) {                   // right after A1 sync bytes look for ID MARK or DAM MARK
        if(decoder.dByte == ID_MARK) {              // if found ID mark, it's a FORMAT TRACK
            decoder.done = true;
            decoder.good = true;
            decoder.isFormatTrack = true;           // the track if being formatted
            Debug::out(LOG_DEBUG, "MfmCachedImage::handleDecodedByte - found ID MARK, assuming track format");
        } else if(decoder.dByte != DAM_MARK) {      // not ID mark (FORMAT TRACK) and not DAM mark (SECTOR WRITE)? fail
            decoder.done = true;
            decoder.good = false;
            Debug::out(LOG_DEBUG, "MfmCachedImage::handleDecodedByte - wrong DAM mark: %02X", decoder.dByte);
        }
    }

    if(decoder.byteOffset >= 1 && decoder.byteOffset <= 512) {  // if we're in the data offset, store data in buffer
        *decoder.oBfr = decoder.dByte;
        decoder.oBfr++;
    }

    if(decoder.byteOffset <= 512) {     // if we're within DAM marker and data
        updateCrcFast(decoder.dByte);   // update CRC
    }

    if(decoder.byteOffset == 513) {     // position of CRC HI? store calced crc and upper part of received crc
        decoder.calcedCrc = crc;
        decoder.recvedCrc = ((uint16_t) decoder.dByte) << 8;
    }

    if(decoder.byteOffset == 514) {     // position of CRC LO? store lower part of received crc and compare to calced crc
        decoder.recvedCrc |= (uint16_t) decoder.dByte;

        decoder.done = true;            // received CRC? nothing more to be done
        decoder.good = (decoder.calcedCrc == decoder.recvedCrc);    // everything good when received and calced crc are th same

        // known issue handling - in some cases the received CRC is different from calculated CRC by 1, but the data is still valid (tested with write-read test on ST)
        // so in this case we pretend that the CRC is fine...
        if((decoder.recvedCrc - decoder.calcedCrc) == 1) {
            Debug::out(LOG_DEBUG, "MfmCachedImage::handleDecodedByte - CRC is off by 1, faking good CRC");
            decoder.good = true;
        }

        int logLevel = decoder.good ? LOG_DEBUG : LOG_ERROR;        // if good then show only on debug log level; if bad then show on error log level
        Debug::out(logLevel, "MfmCachedImage::handleDecodedByte - received CRC: %02x, calculated CRC: %02x, good: %d", decoder.recvedCrc, decoder.calcedCrc, decoder.good);

// uncomment following lines for dumping decoded data to log on error - for manual data inspection
//      if(!decoder.good) {
//          Debug::outBfr(decoder.oBfr - 512, 512);
//      }
    }

    decoder.byteOffset++;
}

bool MfmCachedImage::lastBufferWasFormatTrack(void)
{
    return decoder.isFormatTrack;   // return last state of this flag to tell called if it was sector write or track format
}

bool MfmCachedImage::decodeMfmBuffer(uint8_t *inBfr, int inCnt, uint8_t *outBfr)
{
    // initialize decoder
    decoder.mfmData = inBfr;        // where the mfm data start
    decoder.count = inCnt;          // much much bytes we got
    decoder.pBfr = inBfr;           // where are we decoding now
    decoder.usedTimesFromByte = 0;  // how many times (double-bits) we used from current byte (0..3)
    decoder.oBfr = outBfr;          // where the output data will be stored
    decoder.done = false;           // we're not done yet
    decoder.good = true;            // status returned to caller
    decoder.isFormatTrack = false;  // this will be set if the decoded data seem to be format track stream

    // first loop - find 3x A1 sync symbols
    uint8_t time;
    uint32_t sync = 0;
    bool syncFound = false;
    while(decoder.count >= 0) {     // while there is something in buffer
        time = getMfmTime();

        if(time != MFM_4US && time != MFM_6US && time != MFM_8US) {	// invalid time? skip it
            continue;
        }

        sync = sync << 2;
        sync = sync | time;
        sync = sync & 0x0fffffff;   // leave place only for 3x A1 sync bytes

        if(sync == 0xee7b9ee) {     // 3x A1 found?
            syncFound = true;
            break;
        }
    }

    if(!syncFound) {                // if sync not found, skip the rest
        Debug::out(LOG_ERROR, "MfmCachedImage::decodeMfmBuffer - sync not found");
        return false;
    }

    crc = 0xcdb4;                   // init crc - same as if would init to 0xffff and then update with 3x A1

    decoder.byteOffset = 0;         // what byte offset we have now in the decoded data after 3x A1 marker?
    decoder.bCount = 0;             // how many bits we have now decoded in current byte
    decoder.remainder = false;      // if we got some reminder from previous decoded time

    decoder.dByte = 0;
    while(!decoder.done) {          // still something to decode?
        if(decoder.bCount == 8) {   // got full byte? store and move to next
            handleDecodedByte();    // store byte where it should go
        }

        time = getMfmTime();

        if(decoder.remainder) {     // with remainder of '1' from previous time
            switch(time) {
                case MFM_4US: addOneBit (0, true);  break;
                case MFM_6US: addTwoBits(1, false); break;
                case MFM_8US: addTwoBits(0, true);  break;
            }
        } else {                    // no remainder from previous time
            switch(time) {
                case MFM_4US: addOneBit (1, false); break;
                case MFM_6US: addOneBit (0, true);  break;
                case MFM_8US: addTwoBits(1, false); break;
            }
        }
    }

    return decoder.good;
}

