#ifndef MFMCACHEDIMAGE_H
#define MFMCACHEDIMAGE_H

#include <atomic>
#include "floppyimage.h"

// maximum 2 sides, 85 tracks per side
#define MAX_TRACKS      (2 * 85)
#define MFM_STREAM_SIZE 15000

typedef struct {
    volatile std::atomic<bool> isReady;    // set to false if this track can't be streamed yet (e.g. not encoded yet or encoding at that moment)

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

    void encodeWholeImage(FloppyImage *img);    // go through whole image and encode tracks, returns true if something encoded
    bool findNotReadyTrackAndEncodeIt(FloppyImage *img, int &track, int &side);    // find a single track and encode it, or fail to find it and return false

    bool encodedTrackIsReady(int track, int side);
	BYTE *getEncodedTrack(int track, int side, int &bytesInBuffer);
	bool getParams(int &tracks, int &sides, int &sectorsPerTrack);

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

    TCachedTrack tracks[MAX_TRACKS];
    WORD crc;       // current value of CRC calculator
    BYTE *bfr;      // pointer to where we are storing data in the buffer

    int  getNextIndexToEncode(void);
    void encodeSingleTrack(FloppyImage *img, int side, int track, int sectorsPerTrack);

    void appendCurrentSectorCommand(int track, int side, int sector);
    void appendRawByte(BYTE val);
    void appendA1MarkToStream(void);
    void appendTime(BYTE time);
    void appendByteToStream(BYTE val, bool doCalcCrc=true);
    void appendChange(const BYTE chg);
    bool createMfmStream(FloppyImage *img, int side, int track, int sector);

    void updateCrcSlow(BYTE data);
    void updateCrcFast(BYTE data);

    void dumpTracksToFile(int tracksNo, int sides, int spt);
    void log(char *str);
};

#endif // MFMCACHEDIMAGE_H
