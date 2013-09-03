#ifndef MFMDECODER_H
#define MFMDECODER_H

#include "global.h"
#include "datatypes.h"

class MfmDecoder
{
public:
    MfmDecoder();

    void decodeStream(BYTE *inStream, int inCount, BYTE *outData, int &outCount);

private:
    void addNormalBit(BYTE bit);
    void appendPatternBit(BYTE bit);
    void appendEncodedByte(BYTE val);
    bool appendToSync(WORD val);

    BYTE    *decoded;
    int     index;

    int     bitCount;
    int     patCount;
    BYTE    byte;

    DWORD   a1Cnt;
    DWORD   c2Cnt;
};

#endif // MFMDECODER_H
