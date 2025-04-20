#ifndef MFMDECODER_H
#define MFMDECODER_H

#include "../global.h"
#include <stdint.h>

class MfmDecoder
{
public:
    MfmDecoder();

    void decodeStream(uint8_t *inStream, int inCount, uint8_t *outData, int &outCount);

private:
    void addNormalBit(uint8_t bit);
    void appendPatternBit(uint8_t bit);
    void appendEncodedByte(uint8_t val);
    bool appendToSync(uint16_t val);

    uint8_t    *decoded;
    int     index;

    int     bitCount;
    int     patCount;
    uint8_t    byte;

    uint32_t   a1Cnt;
    uint32_t   c2Cnt;
};

#endif // MFMDECODER_H
