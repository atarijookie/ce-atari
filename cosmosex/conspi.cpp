#include <string.h>

#include "gpio.h"
#include "conspi.h"
#include "global.h"
#include "debug.h"
#include "acsidatatrans.h"
#include "utils.h"

#define SWAP_ENDIAN false

CConSpi::CConSpi()
{
    remainingPacketLength = -1;
}

CConSpi::~CConSpi()
{

}

void CConSpi::applyNoTxRxLimis(int whichSpiCs)
{
    setRemainingTxRxLen(whichSpiCs, NO_REMAINING_LENGTH, NO_REMAINING_LENGTH);  // set no remaining length
}

bool CConSpi::waitForATN(int whichSpiCs, BYTE atnCode, DWORD timeoutMs, BYTE *inBuf)
{
    BYTE outBuf[8];
	
	memset(outBuf, 0, 8);
	memset(inBuf, 0, 8);
	
	// first translate CS signal to ATN signal
	int whichAtnSignal;
	if(whichSpiCs == SPI_CS_HANS) {							// for CS Hans
		whichAtnSignal = SPI_ATN_HANS;						// look for ATN Hans
	} else {												// for CS Franz
		whichAtnSignal = SPI_ATN_FRANZ;						// look for ANT Franz
	}

	applyNoTxRxLimis(whichSpiCs);							// set that we're not limiting the TX/RX count for now
	
	// single check for any ATN code?
	if(atnCode == ATN_ANY) {								// special case - check if any ATN is pending, no wait
		if(!spi_atn(whichAtnSignal)) {						// if that ATN is not up, failed				
			return false;
		}
	
		txRx(whichSpiCs, 8, inBuf, outBuf);					// receive: 0, ATN code, txLen, rxLen
		applyTxRxLimits(whichSpiCs, inBuf);					// now apply txLen and rxLen
		return true;
	}

    // wait for specific ATN code?
    DWORD timeOut = Utils::getEndTime(timeoutMs);

    while(1) {
		if(Utils::getCurrentMs() >= timeOut) {				// if it takes more than allowed timeout, fail
			Debug::out("waitForATN %02x fail - timeout", atnCode);
			return false;
		}
		
		if( spi_atn(whichAtnSignal) ) {						// if ATN signal is up
			break;
		}
		
		Utils::sleepMs(1);									// wait 1 ms
    }

	txRx(whichSpiCs, 8, inBuf, outBuf);					// receive: 0, ATN code, txLen, rxLen
	applyTxRxLimits(whichSpiCs, inBuf);					// now apply txLen and rxLen

    if(inBuf[3] == atnCode) {                      		// ATN code found?
		Debug::out("waitForATN %02x good.", atnCode);
        return true;
	} else {
		Debug::out("waitForATN %02x, but received %02x! Fail!", atnCode, inBuf[3]);
        return false;
	}
}

void CConSpi::applyTxRxLimits(int whichSpiCs, BYTE *inBuff)
{
    WORD *pwIn = (WORD *) inBuff;

	// words 0 and 1 are 0 and ATN code, words 2 and 3 are txLen, rxLen);
    WORD txLen = swapWord(pwIn[2]);
    WORD rxLen = swapWord(pwIn[3]);

    Debug::out("TX/RX limits: TX %d WORDs, RX %d WORDs", txLen, rxLen);

    if(txLen > 1024 || rxLen > 1024) {
        Debug::out("TX/RX limits above are probably wrong! Fix this!");

        if(txLen > 1024) {
            txLen = 1024;
        }

        if(rxLen > 1024) {
            rxLen = 1024;
        }
    }

    setRemainingTxRxLen(whichSpiCs, txLen, rxLen);
}

WORD CConSpi::swapWord(WORD val)
{
    WORD tmp = 0;

    tmp  = val << 8;
    tmp |= val >> 8;

    return tmp;
}

void CConSpi::setRemainingTxRxLen(int whichSpiCs, WORD txLen, WORD rxLen)
{
//    Debug::out("CConSpi::setRemainingTxRxLen - TX %d, RX %d", txLen, rxLen);

    if(txLen == NO_REMAINING_LENGTH && rxLen == NO_REMAINING_LENGTH) {    // if setting NO_REMAINING_LENGTH
        if(remainingPacketLength != 0) {
            Debug::out("CConSpi - didn't TX/RX enough data, padding with %d zeros! Fix this!", remainingPacketLength);
            memset(paddingBuffer, 0, PADDINGBUFFER_SIZE);
            txRx(whichSpiCs, remainingPacketLength, paddingBuffer, paddingBuffer);
        }
    } else {                    // if setting real limit
        txLen *= 2;             // convert WORD count to BYTE count
        rxLen *= 2;

        if(txLen >= 8) {        // if we should TX more than 8 bytes, subtract 8 (header length)
            txLen -= 8;
        } else {                // shouldn't TX 8 or more bytes? Then don't TX anymore.
            txLen = 0;
        }

        if(rxLen >= 8) {        // if we should RX more than 8 bytes, subtract 8 (header length)
            rxLen -= 8;
        } else {                // shouldn't RX 8 or more bytes? Then don't RX anymore.
            rxLen = 0;
        }
    }

    // The SPI bus is TX and RX at the same time, so we will TX/RX until both are used up.
    // So use the greater count as the limit.
    if(txLen >= rxLen) {
        remainingPacketLength = txLen;
    } else {
        remainingPacketLength = rxLen;
    }
}

WORD CConSpi::getRemainingLength(void)
{
    return remainingPacketLength;
}

void CConSpi::txRx(int whichSpiCs, int count, BYTE *sendBuffer, BYTE *receiveBufer)
{
    if(SWAP_ENDIAN) {       // swap endian on sending if required
        BYTE tmp;

        for(int i=0; i<count; i += 2) {
            tmp             = sendBuffer[i+1];
            sendBuffer[i+1] = sendBuffer[i];
            sendBuffer[i]   = tmp;
        }
    }

    if(count == TXRX_COUNT_REST) {          // if should TX/RX the rest, use the remaining length
        count = remainingPacketLength;
    }

    if(remainingPacketLength != NO_REMAINING_LENGTH) {
        if(count > remainingPacketLength) {
            Debug::out("CConSpi::txRx - trying to TX/RX %d more bytes then allowed! Fix this!", (count - remainingPacketLength));

            count = remainingPacketLength;
        }
    }

//    Debug::out("CConSpi::txRx - count: %d", count);

	spi_tx_rx(whichSpiCs, count, sendBuffer, receiveBufer);

    if(remainingPacketLength != NO_REMAINING_LENGTH) {
        remainingPacketLength -= count;             // mark that we've send this much data
    }
}

