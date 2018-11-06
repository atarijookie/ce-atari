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

    // bufferOfBytes -- the datas are transfered as WORDs, but are they stored as bytes?
    // If true, swap bytes, don't append zeros. If false, no swapping, but append zeros.
    void encodeAndCacheImage(FloppyImage *img, bool bufferOfBytes=false);
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

    TCachedTrack tracks[MAX_TRACKS];
    WORD crc;

    void initTracks(void);
    void encodeSingleTrack(FloppyImage *img, int side, int track, int sectorsPerTrack,  BYTE *buffer, int &bytesStored, bool bufferOfBytes=false);

    void appendCurrentSectorCommand(int track, int side, int sector, BYTE *buffer, int &count);
    void appendRawByte(BYTE val, BYTE *bfr, int &cnt);
    void appendZeroIfNeededToMakeEven(BYTE *bfr, int &cnt);
    void appendA1MarkToStream(BYTE *bfr, int &cnt);
    void appendChange(BYTE chg, BYTE *bfr, int &cnt);
    void appendTime(BYTE time, BYTE *bfr, int &cnt);
    void appendByteToStream(BYTE val, BYTE *bfr, int &cnt, bool doCalcCrc=true);
    bool createMfmStream(FloppyImage *img, int side, int track, int sector, BYTE *buffer, int &count);

    void updateCrcSlow(BYTE data);
    void updateCrcFast(BYTE data);
};

#endif // MFMCACHEDIMAGE_H
