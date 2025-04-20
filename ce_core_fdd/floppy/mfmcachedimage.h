#ifndef MFMCACHEDIMAGE_H
#define MFMCACHEDIMAGE_H

#include "floppyimage.h"

// maximum 2 sides, 85 tracks per side
#define MAX_TRACKS      (2 * 85)

// Most of the normal games have tracks from 11000 bytes to 12019 bytes of encoded stream.
// With Bad_Brew_Crew/BBC11.ZIP (11 sectors per track?) the resulting stream was 13205 bytes big, I didn't see anything more yet (but there might be something bigger).
// In theory the 11 sectors per track could be up to 13720 bytes big, so 13800 should be enough here.
// In case something bigger appears (e.g. 12 sectors per track), consider shortening GAP 2, GAP 3a, GAP 3B, GAP 4, which take 774 bytes before encoding, 1548 bytes after encoding,
// so if more space for real data would be needed, some adaptive reducing of GAPs sizes could be added (to a point where WD1772 still considers those GAPs to be valid to catch the sync there)
#define MFM_STREAM_SIZE 13800

typedef struct {
    volatile bool isReady;    // set to false if this track can't be streamed yet (e.g. not encoded yet or encoding at that moment)

    volatile uint32_t  encodeRequestTime;  // timestamp when other thread requested encoding of this track
    volatile uint32_t  encodeActionTime;   // timestamp when encoder did start to encode the track. If at the end of encoding requestTime > actionTime, another request was placed and encode needs to repeat

    int     track;
    int     side;

    uint8_t    *mfmStream;
    int     bytesInStream;

    int     symbolsInStream;            // how many symbols are the result of encoding this single track
    int     totalSymbolsTime;           // sum of symbols time in this track
} TCachedTrack;

class MfmCachedImage
{
public:
    MfmCachedImage();
    virtual ~MfmCachedImage();

    void clearWholeCachedImage(void);           // go and memset() all the cached tracks - used on new image (not on reencode)
    void storeImageParams(FloppyImage *img);
    bool somethingToBeEncoded(void);
    void askToReencodeTrack(int track, int side);

    bool findNotReadyTrackAndEncodeIt(FloppyImage *img, int &track, int &side);    // find a single track and encode it, or fail to find it and return false

    bool encodedTrackIsReady(int track, int side);
    uint8_t *getEncodedTrack(int track, int side, int &bytesInBuffer);
    bool getParams(int &tracks, int &sides, int &sectorsPerTrack);

    bool decodeMfmBuffer(uint8_t *inBfr, int inCnt, uint8_t *outBfr);     // decode single MFM encoded sector
    bool lastBufferWasFormatTrack(void);    // get if last decodeMfmBuffer() was sector write or format track

    bool newContent;

    static void trackAndSideToIndex(const int track, const int side, int &index);
    static void indexToTrackAndSide(const int index, int &track, int &side);

private:
    int nextIndex;      // set this to force encoding of some track sooner than would go

    bool gotImage;

    struct {
        int tracks;
        int sides;
        int spt;
    } params;

    int tracksToBeEncoded;  // holds how many tracks we still need to process

    struct {
        uint8_t threeBits;
        uint8_t times;
        uint8_t timesCnt;

        int symbolsInStream;            // how many symbols are the result of encoding this single track
        int totalSymbolsTime;           // sum of symbols time in this track
    } encoder;

    struct {
        uint8_t *mfmData;
        int   count;

        uint8_t *pBfr;
        int  usedTimesFromByte;

        int  byteOffset;            // offset of decoded byte after 3x A1 mark
        int  bCount;                // how many bits we have now decoded in current byte
        bool remainder;             // if we got some reminder from previous decoded time
        uint8_t dByte;
        uint8_t *oBfr;                 // where the output data will be stored

        uint16_t calcedCrc;
        uint16_t recvedCrc;

        bool done;                  // set to true when received did finish this sector
        bool good;                  // status of sector decoding, which should be returned to caller
        bool isFormatTrack;         // when MFM decoder finds write to ID part of track (not data part), it's format track command
    } decoder;

    int maxTotalSymbolsTime;        // holds the maximum total symbols time found in this image, so we won't show all the tracks symbols time in log
    TCachedTrack tracks[MAX_TRACKS];
    uint16_t crc;       // current value of CRC calculator
    uint8_t *bfr;      // pointer to where we are storing data in the buffer
    uint8_t *currentStreamStart;
    int  bytesInBfr;    // how many bytes we already stored in buffer

    int  getNextIndexToEncode(void);
    void encodeSingleTrack(FloppyImage *img, int side, int track, int sectorsPerTrack);

    void appendCurrentSectorCommand(int track, int side, int sector);
    void appendRawByte(uint8_t val);
    void setRawWordAtIndex(int index, uint16_t val);
    void appendA1MarkToStream(void);
    void appendTime(uint8_t time);
    void appendByteToStream(uint8_t val, bool doCalcCrc=true);
    void appendChange(const uint8_t chg);
    bool encodeSingleSector(FloppyImage *img, int side, int track, int sector);

    void updateCrcSlow(uint8_t data);
    void updateCrcFast(uint8_t data);

    //-------------------
    // methods used for mfm decoding of written sector
    inline uint8_t getMfmTime(void);
    inline void addOneBit(uint8_t bit, bool newRemainder);
    inline void addTwoBits(uint8_t bits, bool newRemainder);
    inline void handleDecodedByte(void);
    //-------------------

    void dumpTracksToFile(int tracksNo, int sides, int spt);
    void log(char *str);
};

#endif // MFMCACHEDIMAGE_H
