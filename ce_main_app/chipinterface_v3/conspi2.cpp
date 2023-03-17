#include <string.h>

#include "../chipinterface_v1_v2/gpio.h"
#include "conspi2.h"
#include "global.h"
#include "debug.h"
#include "acsidatatrans.h"
#include "utils.h"

#define SWAP_ENDIAN false
//#define DEBUG_SPI_COMMUNICATION

#ifdef ONPC
    #define SPI_ATN_HANS    0
    #define SPI_CS_HANS     0
#endif

void chipLog(uint16_t cnt, uint8_t *bfr);

CConSpi2::CConSpi2()
{
    remainingPacketLength = -1;
    paddingBuffer = new uint8_t[PADDINGBUFFER_SIZE];
    memset(paddingBuffer, 0, PADDINGBUFFER_SIZE);
}

CConSpi2::~CConSpi2()
{
    delete [] paddingBuffer;
}

void CConSpi2::setCsForTransfer(bool startNotEnd)
{
#ifndef ONPC
    bcm2835_gpio_write(PIN_CS_HORST, startNotEnd ? LOW : HIGH);     // CS L on start, H on end
#endif
}

void CConSpi2::applyNoTxRxLimis(void)
{
    setRemainingTxRxLen(NO_REMAINING_LENGTH, NO_REMAINING_LENGTH);  // set no remaining length
}

// bool CConSpi2::messageAsRequested(message *, hdd/fdd, uint8_t atnCode)
//{
        // requested ATN_ANY? return whatever we got

        // asking for specific ATN code

        // hdd / fdd + ATN code as requested by called? return it
        // if(inBuf[3] == atnCode) {        // ATN code found?

//}

uint16_t CConSpi2::waitForATN(bool hddNotFdd, uint8_t atnCode, uint32_t timeoutMs, uint8_t *inBuf)
{
    // start the timeout count
    uint32_t timeOut = Utils::getEndTime(timeoutMs);

    while(true) {
        // rx_queue not empty? go through the messages, if any of them is is matching interface + ATN, return it
        // for message in received_queue
        //      if CConSpi2::messageAsRequested(item)
        //          return marker;

        uint16_t marker = transferPacket(timeoutMs);

        // IKBD data?
        if(marker == MARKER_IKBD) {
            // TODO: send to IKBD thread
            continue;
        }

        // logs?
        if(marker == MARKER_LOGS) {
            // TODO: write to log file
            // chipLog(len, &cbReceivedData);
            continue;
        }

        // if CConSpi2::messageAsRequested(new_item)
        //      return marker;

        // hdd / fdd + ATN code different to what was requested? add to rx_queue

        // nothing more to TX and we're out of time? quit
//        if tx_queue.empty && Utils::getCurrentMs() >= timeOut) {
//            return 0;
//        }
    }
}

uint16_t CConSpi2::transferPacket(uint32_t timeoutMs)
{
    memset(txBuffer, 0, 8);
    memset(rxBuffer, 0, 8);

    applyNoTxRxLimis();                                 // set that we're not limiting the TX/RX count for now

    // HOST INITIALIZED TRANSFER:
    // if ATN is L, CE is not requesting transfer, and TX queue is not empty - RPi should now request transfer
    bool itemInTxQeueue = false;                        // TODO: actual checking of TX queue

    if(!spi_atn(SPI_ATN_HANS) && itemInTxQeueue) {      // ATN is L, but something to TX to CE
        setCsForTransfer(true);                         // put CS to L

        // wait for ATN going H
        uint32_t timeOut = Utils::getEndTime(100);      // wait some time for ATN to go high

        while(1) {
            if(Utils::getCurrentMs() >= timeOut) {      // if it takes more than allowed timeout, fail
                setCsForTransfer(false);                // put CS back to H
                Debug::out(LOG_ERROR, "waitForATN - timeout on HOST INITIALIZED TRANSFER");
                return false;
            }

            if(spi_atn(SPI_ATN_HANS) ) {               // if ATN signal is up, we can quit waiting
                break;
            }
        }
    }

    // ATN is H in this now - either we've just requested transfer, or CE was requesting transfer before us
    setCsForTransfer(true);                             // put CS to L

    // read header
    uint16_t marker = readHeader();
    if(!marker) {        // receive: 0xcafe, ATN code, txLen, rxLen
        setCsForTransfer(false);                        // put CS back to H
        return 0;
    }

    // fetch item to TX to CE

    // get whole transfer size = max(incoming_from_ce, size_of_outgoing_message)

    // TX and RX message

    return marker;      // now we return what marker was found
}

uint16_t CConSpi2::readHeader(void)
{
    uint16_t marker = 0;
    uint16_t *inWord = (uint16_t *) rxBuffer;
    uint32_t loops = 0;

    // read the first uint16_t, if it's not 0xcafe, then read again to synchronize
    while(sigintReceived == 0) {
        txRx(2, txBuffer, rxBuffer);            // receive: 0, ATN code, txLen, rxLen
        marker = *inWord;

        // if found one of the supported markers
        if(marker == MARKER_HDD || marker == MARKER_FDD || marker == MARKER_IKBD ||  marker == MARKER_LOGS) {
            break;
        }

        loops++;

        if(loops >= 10000) {                    // if this doesn't synchronize in 10k loops, something is very wrong
            Debug::out(LOG_ERROR, "readHeader couldn't synchronize!");
            return 0;                           // return value of 0 means no marker found
        }
    }

    if(loops != 0) {
        Debug::out(LOG_DEBUG, "readHeader took %d loops to synchronize!", loops);
    }

    txRx(6, txBuffer + 2, rxBuffer + 2);        // receive: 0, ATN code, txLen, rxLen
    applyTxRxLimits(rxBuffer);                  // now apply txLen and rxLen

    return marker;
}

void CConSpi2::applyTxRxLimits(uint8_t *inBuff)
{
    uint16_t *pwIn = (uint16_t *) inBuff;

    // words 0 and 1 are 0 and ATN code, words 2 and 3 are txLen, rxLen);
    uint16_t txLen = swapWord(pwIn[2]);
    uint16_t rxLen = swapWord(pwIn[3]);

#ifdef DEBUG_SPI_COMMUNICATION
    Debug::out(LOG_DEBUG, "TX/RX limits: TX %d WORDs, RX %d WORDs", txLen, rxLen);
#endif

    if(txLen > TWENTY_KB || rxLen > TWENTY_KB) {
        Debug::out(LOG_ERROR, "applyTxRxLimits - TX/RX limits for FRANZ are probably wrong! Fix this!");
        txLen = MIN(txLen, TWENTY_KB);
        rxLen = MIN(rxLen, TWENTY_KB);
    }

    setRemainingTxRxLen(txLen, rxLen);
}

uint16_t CConSpi2::swapWord(uint16_t val)
{
    uint16_t tmp = 0;

    tmp  = val << 8;
    tmp |= val >> 8;

    return tmp;
}

void CConSpi2::setRemainingTxRxLen(uint16_t txLen, uint16_t rxLen)
{
#ifdef DEBUG_SPI_COMMUNICATION
    if(txLen != NO_REMAINING_LENGTH || rxLen != NO_REMAINING_LENGTH || remainingPacketLength != NO_REMAINING_LENGTH) {
        Debug::out(LOG_DEBUG, "CConSpi2::setRemainingTxRxLen - TX %d, RX %d, while the remainingPacketLength is %d", txLen, rxLen, remainingPacketLength);
    }
#endif

    if(txLen == NO_REMAINING_LENGTH && rxLen == NO_REMAINING_LENGTH) {      // if setting NO_REMAINING_LENGTH
        if(remainingPacketLength != 0 && remainingPacketLength != NO_REMAINING_LENGTH) {
            int remLen = MIN(remainingPacketLength, PADDINGBUFFER_SIZE);    // if trying to tx/rx more than size of padding buffer (which is most probably wrong), limit it padding buffer size

            Debug::out(LOG_ERROR, "CConSpi2 - didn't TX/RX enough data, padding with %d zeros! Fix this!", remLen);
            txRx(remLen, paddingBuffer, paddingBuffer);
        }
    } else {                    // if setting real limit
        txLen *= 2;             // convert uint16_t count to uint8_t count
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

uint16_t CConSpi2::getRemainingLength(void)
{
    return remainingPacketLength;
}

void CConSpi2::txRx(int count, uint8_t *sendBuffer, uint8_t *receiveBufer)
{
    if(SWAP_ENDIAN) {       // swap endian on sending if required
        uint8_t tmp;

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
            Debug::out(LOG_ERROR, "CConSpi2::txRx - trying to TX/RX %d more bytes then allowed! Fix this!", (count - remainingPacketLength));

            count = remainingPacketLength;
        }
    }

#ifdef DEBUG_SPI_COMMUNICATION
    Debug::out(LOG_DEBUG, "CConSpi2::txRx - count: %d", count);
#endif

    spi_tx_rx(SPI_CS_HANS, count, sendBuffer, receiveBufer);

    if(remainingPacketLength != NO_REMAINING_LENGTH) {
        remainingPacketLength -= count;             // mark that we've send this much data
    }
}
