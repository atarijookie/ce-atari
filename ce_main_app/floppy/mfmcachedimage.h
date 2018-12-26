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

    volatile DWORD  encodeRequestTime;  // timestamp when other thread requested encoding of this track
    volatile DWORD  encodeActionTime;   // timestamp when encoder did start to encode the track. If at the end of encoding requestTime > actionTime, another request was placed and encode needs to repeat

    int     track;
    int     side;

    BYTE    *mfmStream;
    int     bytesInStream;
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
    BYTE *getEncodedTrack(int track, int side, int &bytesInBuffer);
    bool getParams(int &tracks, int &sides, int &sectorsPerTrack);

    bool decodeMfmBuffer(BYTE *inBfr, int inCnt, BYTE *outBfr);     // decode single MFM encoded sector

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
        BYTE threeBits;
        BYTE times;
        BYTE timesCnt;
    } encoder;

    struct {
        BYTE *mfmData;
        int   count;

        BYTE *pBfr;
        int  usedTimesFromByte;

        int  byteOffset;            // offset of decoded byte after 3x A1 mark
        int  bCount;                // how many bits we have now decoded in current byte
        bool remainder;             // if we got some reminder from previous decoded time
        BYTE dByte;
        BYTE *oBfr;                 // where the output data will be stored

        WORD calcedCrc;
        WORD recvedCrc;

        bool done;                  // set to true when received did finish this sector
        bool good;                  // status of sector decoding, which should be returned to caller
    } decoder;

    TCachedTrack tracks[MAX_TRACKS];
    WORD crc;       // current value of CRC calculator
    BYTE *bfr;      // pointer to where we are storing data in the buffer
    BYTE *currentStreamStart;
    int  bytesInBfr;    // how many bytes we already stored in buffer

    int  getNextIndexToEncode(void);
    void encodeSingleTrack(FloppyImage *img, int side, int track, int sectorsPerTrack);

    void appendCurrentSectorCommand(int track, int side, int sector);
    void appendRawByte(BYTE val);
    void setRawWordAtIndex(int index, WORD val);
    void appendA1MarkToStream(void);
    void appendTime(BYTE time);
    void appendByteToStream(BYTE val, bool doCalcCrc=true);
    void appendChange(const BYTE chg);
    bool encodeSingleSector(FloppyImage *img, int side, int track, int sector);

    void updateCrcSlow(BYTE data);
    void updateCrcFast(BYTE data);

    //-------------------
    // methods used for mfm decoding of written sector
    inline BYTE getMfmTime(void);
    inline void addOneBit(BYTE bit, bool newRemainder);
    inline void addTwoBits(BYTE bits, bool newRemainder);
    inline void handleDecodedByte(void);
    //-------------------

    void dumpTracksToFile(int tracksNo, int sides, int spt);
    void log(char *str);
};

#endif // MFMCACHEDIMAGE_H
