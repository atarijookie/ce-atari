#ifndef CONSPI_H
#define CONSPI_H

#include "datatypes.h"

#define NO_REMAINING_LENGTH     -1
#define TXRX_COUNT_REST         -1
#define PADDINGBUFFER_SIZE      1024

class CConSpi
{
public:
    CConSpi();
    ~CConSpi();

    void txRx(int whichSpiCs, int count, BYTE *sendBuffer, BYTE *receiveBufer);

    void receiveAndApplyTxRxLimits(int whichSpiCs);
    void applyNoTxRxLimis(int whichSpiCs);
    void setRemainingTxRxLen(int whichSpiCs, WORD txLen, WORD rxLen);
    WORD getRemainingLength(void);

private:
    WORD remainingPacketLength;
    BYTE paddingBuffer[PADDINGBUFFER_SIZE];

    WORD swapWord(WORD val);
};

#endif // CONSPI_H
