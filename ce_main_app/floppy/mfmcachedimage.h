#ifndef MFMCACHEDIMAGE_H
#define MFMCACHEDIMAGE_H

#include "floppyimage.h"

// maximum 2 sides, 85 tracks per side
#define MAX_TRACKS      (2 * 85)

typedef struct {
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

    void encodeAndCacheImage(FloppyImage *img);
    void deleteCachedImage(void);

	BYTE *getEncodedTrack(int track, int side, int &bytesInBuffer);
	bool getParams(int &tracks, int &sides, int &sectorsPerTrack);

	void copyFromOther(MfmCachedImage &other);
    bool newContent;

private:
    bool gotImage;

	struct {
		int tracks;
		int sides;
		int spt;
	} params;

    struct {
        BYTE threeBits;
        BYTE times;
        BYTE timesCnt;
    } encoder;

    TCachedTrack tracks[MAX_TRACKS];
    WORD crc;

    BYTE encodeBuffer[20480];   // buffer where the encoding of single track will happen
    BYTE *bfr;                  // pointer to where we are in the buffer

    void initTracks(void);
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
