#ifndef CONSPI_H
#define CONSPI_H

#include "datatypes.h"

#define NO_REMAINING_LENGTH     0xffff
#define TXRX_COUNT_REST         0xffff
#define PADDINGBUFFER_SIZE      1024

class CConSpi
{
public:
    CConSpi();
    ~CConSpi();

	bool waitForATN(int whichSpiCs, BYTE atnCode, DWORD timeoutMs, BYTE *inBuf);
    void txRx(int whichSpiCs, int count, BYTE *sendBuffer, BYTE *receiveBufer);

	void applyTxRxLimits(int whichSpiCs, BYTE *inBuff);
	void applyNoTxRxLimis(int whichSpiCs);
		
    void setRemainingTxRxLen(int whichSpiCs, WORD txLen, WORD rxLen);
    WORD getRemainingLength(void);

private:
    WORD remainingPacketLength;
    BYTE paddingBuffer[PADDINGBUFFER_SIZE];

	bool readHeader(int whichSpiCs, BYTE *outBuf, BYTE *inBuf);	
    WORD swapWord(WORD val);
};

#endif // CONSPI_H
