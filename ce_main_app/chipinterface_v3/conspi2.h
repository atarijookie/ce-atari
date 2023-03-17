#ifndef CONSPI2_H
#define CONSPI2_H

#include <stdint.h>

#define NO_REMAINING_LENGTH     0xffff
#define TXRX_COUNT_REST         0xffff

#define ONE_KB                  1024
#define TWENTY_KB               (20 * 1024)
#define PADDINGBUFFER_SIZE      0xffff

// v3 uses single stream, so markers for each stream are different.
// They are all different from v1/v2 marker, so the v1/v2 chip interface code will fail with v3 protocol.
#define MARKER_HDD      0xfdca      // cafd - hdD
#define MARKER_FDD      0xffca      // caff - Fdd
#define MARKER_IKBD     0xfbca      // cafb - ikBd
#define MARKER_LOGS     0xf0ca      // caf0 - l0gs

// Horst chip in v3 uses different CS than Franz and Hans, because we want to have complete control over the CS line
#define PIN_CS_HORST    PIN_TX_SEL1N2

class CConSpi2
{
public:
    CConSpi2();
    ~CConSpi2();

    void setCsForTransfer(bool startNotEnd);

    uint16_t waitForATN(bool hddNotFdd, uint8_t atnCode, uint32_t timeoutMs, uint8_t *inBuf);
    void txRx(int count, uint8_t *sendBuffer, uint8_t *receiveBufer);

    uint16_t getRemainingLength(void);

private:
    uint8_t txBuffer[TWENTY_KB];
    uint8_t rxBuffer[TWENTY_KB];

    uint16_t remainingPacketLength;
    uint8_t *paddingBuffer;

    uint16_t readHeader(void);
    uint16_t swapWord(uint16_t val);

    void applyTxRxLimits(uint8_t *inBuff);
    void applyNoTxRxLimis(void);

    void setRemainingTxRxLen(uint16_t txLen, uint16_t rxLen);
    uint16_t transferPacket(uint32_t timeoutMs);
};

#endif // CONSPI2_H
