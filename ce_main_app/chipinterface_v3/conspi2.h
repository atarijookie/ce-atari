#ifndef CONSPI2_H
#define CONSPI2_H

#include <stdint.h>

#define NO_REMAINING_LENGTH     0xffff
#define TXRX_COUNT_REST         0xffff

#define ONE_KB                  1024
#define TWENTY_KB               (20 * 1024)
#define PADDINGBUFFER_SIZE      0xffff

class CConSpi2
{
public:
    CConSpi2();
    ~CConSpi2();

    bool waitForATN(int whichSpiCs, uint8_t atnCode, uint32_t timeoutMs, uint8_t *inBuf);
    void txRx(int whichSpiCs, int count, uint8_t *sendBuffer, uint8_t *receiveBufer);

    uint16_t getRemainingLength(void);

private:
    uint16_t remainingPacketLength;
    uint8_t *paddingBuffer;

    bool readHeader(int whichSpiCs, uint8_t *outBuf, uint8_t *inBuf);
    uint16_t swapWord(uint16_t val);

    void applyTxRxLimits(int whichSpiCs, uint8_t *inBuff);
    void applyNoTxRxLimis(int whichSpiCs);

    void setRemainingTxRxLen(int whichSpiCs, uint16_t txLen, uint16_t rxLen);
};

#endif // CONSPI2_H
