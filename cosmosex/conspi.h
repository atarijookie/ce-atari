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

    virtual DWORD   bytesToReceive(void);
    virtual DWORD   bytesToSend(void);
    virtual void    txRx(int count, BYTE *sendBuffer, BYTE *receiveBufer, bool addLastToAtn=true);
    virtual void    write(int count, BYTE *buffer);
    virtual void    read (int count, BYTE *buffer);

    void receiveAndApplyTxRxLimits(void);
    void applyNoTxRxLimis(void);
    void setRemainingTxRxLen(WORD txLen, WORD rxLen);
    WORD getRemainingLength(void);

    void getAtnWord(BYTE *bfr);
    void setAtnWord(BYTE *bfr);

private:
    void zeroAllVars(void);

    WORD                        remainingPacketLength;
    BYTE                        paddingBuffer[PADDINGBUFFER_SIZE];

    struct {
        bool got;
        BYTE bytes[2];
    } prevAtnWord;

    WORD swapWord(WORD val);
};

#endif // CONSPI_H
