#include <string.h>
#include <unistd.h>

#include "../chipinterface_v1_v2/gpio.h"
#include "conspi2.h"
#include "global.h"
#include "debug.h"
#include "acsidatatrans.h"
#include "utils.h"

//#define DEBUG_SPI_COMMUNICATION

#ifdef ONPC
    #define SPI_ATN_HANS    0
    #define SPI_CS_HANS     0
#endif

void chipLog(uint16_t cnt, uint8_t *bfr);

CConSpi2::CConSpi2()
{
    ikbdWriteFd = -1;

    // construct dummy packet, which we will TX, when there's nothing to TX, but we want to RX
    txDummyPacket = new SpiTxPacket(MARKER_HDD, CMD_DUMMY, 0, NULL);

    pthread_mutex_init(&mutex, NULL);
}

CConSpi2::~CConSpi2()
{
    delete txDummyPacket;
}

void CConSpi2::setCsForTransfer(bool startNotEnd)
{
#ifndef ONPC
    bcm2835_gpio_write(PIN_CS_HORST, startNotEnd ? LOW : HIGH);     // CS L on start, H on end
#endif
}

void CConSpi2::addToTxQueue(uint16_t marker, uint16_t cmd, uint16_t dataSize, uint8_t* inData)
{
    pthread_mutex_lock(&mutex);

    // add copy of this data TX queue
    SpiTxPacket *txPacket = new SpiTxPacket(marker, cmd, dataSize, inData);
    txQueue.push(txPacket);

    pthread_mutex_unlock(&mutex);
}

SpiRxPacket* CConSpi2::waitForATN(uint8_t expectMarker, uint8_t atnCode, uint32_t timeoutMs)
{
    // start the timeout count
    uint32_t timeOut = Utils::getEndTime(timeoutMs);
    SpiRxPacket rxPacket;

    while(true) {
        // rxQueue not empty? go through the messages, if any of them is is matching interface + ATN, return it
        std::list<SpiRxPacket *>::iterator it;
        for (it = rxQueue.begin(); it != rxQueue.end(); ++it) {     // go through the queue
            SpiRxPacket* rxPack = (*it);                            // dereference iterator to packet pointer
            if(rxPack->asRequested(expectMarker, atnCode)) {        // is this packet from queue what we are looking for?
                rxQueue.remove(rxPack);     // remove this packet from queue, this messes up iterators, but we will quit here, so it shouldn't matter
                return rxPack;              // return pointer to this packet
            }
        }

        uint16_t marker = transferPacket();

        if(marker) {        // some valid marker was found? use current rx buffer as rx packet
            rxPacket.useRawData(rxBuffer, false);
        }

        if(marker == MARKER_IKBD || marker == MARKER_LOGS) {    // IKBD data or logs?
            uint8_t* buffer = rxPacket.getDataPointer();
            uint32_t length = rxPacket.getDataSize();

            if(marker == MARKER_IKBD && ikbdWriteFd > 0) {      // it's for IKBD and got IKBD pipe?
                write(ikbdWriteFd, buffer, length);
            }

            if(marker == MARKER_LOGS) {                         // it's logs? write them to file
                chipLog(length, buffer);
            }

            continue;
        }

        if(marker) {        // this is a valid marker and it's not IKBD and LOGS, so it's HDD or FDD
            SpiRxPacket *rxPacket2 = new SpiRxPacket();     // create new RX packet object
            rxPacket2->useRawData(rxBuffer, true);          // copy data from rxBuffer

            if(rxPacket.asRequested(expectMarker, atnCode)) {  // is this the packet we've requested? return it
                return rxPacket2;
            } else {                                // now what we've requested? add to queue
                rxQueue.push_front(rxPacket2);
            }
        }

        pthread_mutex_lock(&mutex);
        bool txQueueEmpty = txQueue.empty();
        pthread_mutex_unlock(&mutex);

        // nothing more to TX and we're out of time? quit
        if(txQueueEmpty && Utils::getCurrentMs() >= timeOut) {
            return NULL;
        }
    }
}

uint16_t CConSpi2::transferPacket(void)
{
    pthread_mutex_lock(&mutex);
    bool txQueueEmpty = txQueue.empty();
    pthread_mutex_unlock(&mutex);

    bool atnIsH = spi_atn(SPI_ATN_HANS);

    // ATN is L (CE doesn't need to TX) and our TX queue is empty (we don't need to TX)
    if(!atnIsH && txQueueEmpty) {
        return 0;
    }

    memset(txBuffer, 0, 8);
    memset(rxBuffer, 0, 8);

    // HOST INITIALIZED TRANSFER - we can start HOST initialized transfer if ATN is L.
    // We want to start HOST initialized transfer, if we got something to TX (tx queue not empty).
    if(!atnIsH && !txQueueEmpty) {                      // ATN is L, but something to TX to CE
        setCsForTransfer(true);                         // put CS to L

        // wait for ATN going H
        uint32_t timeOut = Utils::getEndTime(100);      // wait some time for ATN to go high

        while(1) {
            if(Utils::getCurrentMs() >= timeOut) {      // if it takes more than allowed timeout, fail
                setCsForTransfer(false);                // put CS back to H
                Debug::out(LOG_ERROR, "waitForATN - timeout on HOST INITIALIZED TRANSFER");
                return 0;
            }

            if(spi_atn(SPI_ATN_HANS) ) {    // if ATN signal is up, we can quit waiting
                break;
            }
        }
    }

    // ATN is H in this now - either we've just requested transfer, or CE was requesting transfer before us
    setCsForTransfer(true);                 // put CS to L

    // read header
    uint16_t marker = readHeader();         // receive: 0xcaf*, ATN code, txLen, rxLen (8 bytes)
    if(!marker) {                           // no recognized header was found? quit
        setCsForTransfer(false);            // put CS back to H
        return 0;
    }

    // fetch item to TX to CE
    SpiTxPacket *txPacket = txDummyPacket;

    if(!txQueueEmpty) {                     // got something in TX queue?
        pthread_mutex_lock(&mutex);
        txPacket = txQueue.front();         // get pointer
        txQueue.pop();                      // remove pointer from queue
        pthread_mutex_unlock(&mutex);
    }

    // get whole transfer size = max(incoming_from_ce, size_of_outgoing_message)
    SpiRxPacket rxPacket;
    rxPacket.useRawData(rxBuffer, false);   // use SpiRxPacket to read data size as the header is already present (data is still to be transferred)

    uint16_t txRxLen = MAX(rxPacket.getDataSize() + 2, txPacket->size); // max(incoming_from_ce, size_of_outgoing_message)

    // TX and RX message
    spi_tx_rx(SPI_CS_HANS, txRxLen, txPacket->data, rxBuffer + 8);    // rx buffer contains received data including header

    if(txPacket != txDummyPacket) {     // if this is not the dummy packet, free it the memory
        delete txPacket;
    }

    setCsForTransfer(false);            // put CS back to H
    return marker;                      // now we return what marker was found
}

uint16_t CConSpi2::readHeader(void)
{
    uint16_t marker = 0;
    uint16_t *inWord = (uint16_t *) rxBuffer;
    uint32_t loops = 0;

    // read the first uint16_t, if it's not 0xcafe, then read again to synchronize
    while(sigintReceived == 0) {
        spi_tx_rx(SPI_CS_HANS, 2, txBuffer, rxBuffer);            // receive just marker
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

    spi_tx_rx(SPI_CS_HANS, 6, txBuffer + 2, rxBuffer + 2);        // receive: ATN code, txLen, rxLen
    return marker;
}

void CConSpi2::getTxSize(uint8_t* inBuff, uint16_t& txLen)
{
    uint16_t *pwIn = (uint16_t *) inBuff;

    // words 0 and 1 are 0 and ATN code, words 2 and 3 are txLen, rxLen);
    txLen = swapWord(pwIn[2]);
    txLen *= 2;                     // word count to byte count

#ifdef DEBUG_SPI_COMMUNICATION
    Debug::out(LOG_DEBUG, "getTxSize: TX %d WORDs", txLen);
#endif

    if(txLen > SPI_TX_RX_BFR_SIZE) {
        Debug::out(LOG_ERROR, "getTxSize: TX limits for Horst are probably wrong! Fix this!");
        txLen = MIN(txLen, SPI_TX_RX_BFR_SIZE);
    }
}

uint16_t CConSpi2::swapWord(uint16_t val)
{
    uint16_t tmp = 0;

    tmp  = val << 8;
    tmp |= val >> 8;

    return tmp;
}
