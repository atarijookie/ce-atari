#include <string.h>
#include <unistd.h>

#include "../chipinterface_v1_v2/gpio.h"
#include "conspi2.h"
#include "global.h"
#include "debug.h"
#include "acsidatatrans.h"
#include "utils.h"

#ifdef ONPC
    #define SPI_ATN_HANS    0
    #define SPI_CS_HANS     0
#endif

CConSpi2::CConSpi2()
{
    ikbdWriteFd = -1;

    // construct dummy packet, which we will TX, when there's nothing to TX, but we want to RX
    txDummyPacket = new SpiTxPacket(MARKER_HDD, CMD_DUMMY, 0, NULL);

    pthread_mutex_init(&mutex, NULL);

    testEndian();
}

CConSpi2::~CConSpi2()
{
    delete txDummyPacket;
}

void CConSpi2::testEndian(void)
{
    uint8_t bfrBytes[8];
    Utils::storeWord(bfrBytes, 0x1122);         // store WORD to buffer - 0x11 first, 0x22 next
    uint16_t *inWord = (uint16_t *) bfrBytes;   // cast BYTE pointer to WORD

    // get value like chip interface v1/v2 - by casting BYTE* to WORD* and reading, expecting the order to be reversed to be good
    uint16_t marker = *inWord;
    Debug::out(LOG_INFO, "CConSpi2::testEndian 1 - order %s", (marker == 0x2211) ? "OK" : "BAD");

    // get value and use swapWord like chip interface v1/v2 and expect the correct order
    uint16_t swapped = Utils::SWAPWORD2(inWord[0]);
    Debug::out(LOG_INFO, "CConSpi2::testEndian 2 - order %s", (swapped == 0x1122) ? "OK" : "BAD");

    // get value using Utils::getWord() and expect correct order
    uint16_t getted = Utils::getWord(bfrBytes);
    Debug::out(LOG_INFO, "CConSpi2::testEndian 3 - order %s", (getted == 0x1122) ? "OK" : "BAD");
}

void CConSpi2::setCsForTransfer(bool startNotEnd)
{
#ifndef ONPC
    bcm2835_gpio_write(PIN_CS_HORST, startNotEnd ? LOW : HIGH);     // CS L on start, H on end
#endif
}

void CConSpi2::addToTxQueue(uint16_t marker, uint16_t cmd, uint16_t dataSizeInBytes, uint8_t* inData)
{
    pthread_mutex_lock(&mutex);

#ifdef DEBUG_SPI_COMMUNICATION
    Debug::out(LOG_DEBUG, "addToTxQueue - marker: %04X, cmd: %04X, dataSizeInBytes: %d", marker, cmd, dataSizeInBytes);
    Debug::outBfr(inData, dataSizeInBytes);
#endif

    // add copy of this data TX queue
    SpiTxPacket *txPacket = new SpiTxPacket(marker, cmd, dataSizeInBytes, inData);
    txQueue.push(txPacket);

    pthread_mutex_unlock(&mutex);
}

SpiRxPacket* CConSpi2::waitForATN(uint8_t expectMarker, uint8_t atnCode, uint32_t timeoutMs)
{
    // start the timeout count
    uint32_t timeOut = Utils::getEndTime(timeoutMs);
    SpiRxPacket rxPacket;
    int maxLoops = 10;      // limit this wait loop to some count of attempts

    while(sigintReceived == 0 && maxLoops > 0) {
        // To make sure this loop still quits after some time, then if the timeoutMs is:
        // A) 0 ms - limit to maxLoops counts to enable sending and receiving some packets which are waiting for transfer
        // B) >0 ms - limit this loop by the specified timeout (thus don't decrement maxLoops count here)
        if(timeoutMs == 0) {
            maxLoops--;
        }

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
#ifdef DEBUG_SPI_COMMUNICATION
            Debug::out(LOG_DEBUG, "waitForATN - marker: %04X", marker);
#endif
            rxPacket.useRawData(rxBuffer, false);
        }

        if(marker == MARKER_IKBD) {     // IKBD data?
            uint8_t* buffer = rxPacket.getDataPointer();
            uint32_t length = rxPacket.getDataSize();

            rxPacket.logPacket();

            if(ikbdWriteFd > 0) {       // got IKBD pipe?
                write(ikbdWriteFd, buffer, length);
            }

#ifdef DEBUG_SPI_COMMUNICATION
            Debug::out(LOG_DEBUG, "waitForATN - MARKER_IKBD was processed, continue");
#endif
            continue;
        }

        if(marker) {        // this is a valid marker and it's not IKBD and LOGS, so it's HDD or FDD
            SpiRxPacket *rxPacket2 = new SpiRxPacket();     // create new RX packet object
            rxPacket2->useRawData(rxBuffer, true);          // copy data from rxBuffer
            rxPacket2->logPacket();

            if(rxPacket.asRequested(expectMarker, atnCode)) {  // is this the packet we've requested? return it
#ifdef DEBUG_SPI_COMMUNICATION
                Debug::out(LOG_DEBUG, "waitForATN - asRequested() - YES, return");
#endif
                return rxPacket2;
            } else {                                // now what we've requested? add to queue
#ifdef DEBUG_SPI_COMMUNICATION
                Debug::out(LOG_DEBUG, "waitForATN - asRequested() - NO, push");
#endif
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

    return NULL;
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
    memset(rxBuffer, 0, 32);

    // HOST INITIALIZED TRANSFER - we can start HOST initialized transfer if ATN is L.
    // We want to start HOST initialized transfer, if we got something to TX (tx queue not empty).
    if(!atnIsH && !txQueueEmpty) {                      // ATN is L, but something to TX to CE
        setCsForTransfer(true);                         // put CS to L

        // wait for ATN going H
        // TODO: change to 100 ms after fixed
        uint32_t timeOut = Utils::getEndTime(5000);     // wait some time for ATN to go high

        while(sigintReceived == 0) {
            if(Utils::getCurrentMs() >= timeOut) {      // if it takes more than allowed timeout, fail
                setCsForTransfer(false);                // put CS back to H
                Debug::out(LOG_ERROR, "transferPacket - timeout on HOST INITIALIZED TRANSFER");
                return 0;
            }

            if(spi_atn(SPI_ATN_HANS) ) {    // if ATN signal is up, we can quit waiting
                break;
            }
        }
    }

    // ATN is H in this now - either we've just requested transfer, or CE was requesting transfer before us
    setCsForTransfer(true);                 // put CS to L

    atnIsH = spi_atn(SPI_ATN_HANS);
    if(!atnIsH) {
        Debug::out(LOG_ERROR, "transferPacket - ATN not H before transfer, this is wrong!");
    }

    // read header
    uint16_t marker = readHeader();         // receive: 0xcaf*, ATN code, txLen, rxLen (8 bytes)
    if(!marker) {                           // no recognized header was found? quit
        setCsForTransfer(false);            // put CS back to H
#ifdef DEBUG_SPI_COMMUNICATION
        Debug::out(LOG_DEBUG, "transferPacket - no marker");
#endif
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
#ifdef DEBUG_SPI_COMMUNICATION
    Debug::out(LOG_DEBUG, "transferPacket - rxPacket size: %d, txPacket size: %d, so txRxLen is: %d", rxPacket.getDataSize(), txPacket->size, txRxLen);
#endif

    // TX and RX message
    spi_tx_rx(SPI_CS_HANS, txRxLen, txPacket->data, rxBuffer + 8);    // rx buffer contains received data including header

    if(txPacket != txDummyPacket) {     // if this is not the dummy packet, free it the memory
        delete txPacket;
    }

    if(rxPacket.atnCode() == CMD_DUMMY) {   // if this is just dummy packet, ignore it (return 'no marker found')
#ifdef DEBUG_SPI_COMMUNICATION
        Debug::out(LOG_DEBUG, "transferPacket - rxPacket with CMD_DUMMY ignored.");
#endif
        marker = 0;
    } else {
#ifdef DEBUG_SPI_COMMUNICATION
        Debug::out(LOG_DEBUG, "transferPacket - rxPacket - marker: %04X, ATN: %04X", rxPacket.marker(), rxPacket.atnCode());
#endif
    }

    setCsForTransfer(false);            // put CS back to H
#ifdef DEBUG_SPI_COMMUNICATION
    Debug::out(LOG_DEBUG, "END OF PACKET RECEIVING ^^^");
    // after this point the packet is fully received into this device
#endif
    return marker;                      // now we return what marker was found
}

uint16_t CConSpi2::readHeader(void)
{
    uint16_t marker = 0;
    uint32_t loops = 0;
    uint32_t cntZero = 0, cntNonZero = 0;

    memset(txBuffer, 0, 8);

    // read the first uint16_t, if it's not 0xcafe, then read again to synchronize
    while(sigintReceived == 0) {
        spi_tx_rx(SPI_CS_HANS, 2, txBuffer, rxBuffer);          // receive just marker, send zeros
        marker = Utils::getWord(rxBuffer);
        (marker == 0) ? (cntZero++) : (cntNonZero++);           // update counts

        // if found one of the supported markers
        if(marker == MARKER_HDD || marker == MARKER_FDD || marker == MARKER_IKBD) {
            break;
        }

        loops++;

        if(loops >= 10000) {                    // if this doesn't synchronize in 10k loops, something is very wrong
            Debug::out(LOG_ERROR, "readHeader couldn't synchronize! Found zeros: %d, found other: %d", cntZero, cntNonZero);
            return 0;                           // return value of 0 means no marker found
        }
    }

    if(loops != 0) {
        Debug::out(LOG_DEBUG, "readHeader took %d loops to synchronize!", loops);
    }

#ifdef DEBUG_SPI_COMMUNICATION
    Debug::out(LOG_DEBUG, "START OF PACKET RECEIVING vvv");
    Debug::out(LOG_DEBUG, "readHeader found marker %04X", marker);
#endif

    spi_tx_rx(SPI_CS_HANS, 6, txBuffer, rxBuffer + 2);        // receive: ATN code, txLen, invertedMarker, send zeros

    uint16_t invMarkerGot = Utils::getWord(rxBuffer + 6);
    uint16_t invMarkerExpect = ~marker;

    if(invMarkerGot != invMarkerExpect) {       // check the inverted marker to be the inversion of marker
        Debug::out(LOG_WARNING, "readHeader - invMarker was expected to be %04X, but it's %04X instead!", invMarkerExpect, invMarkerGot);
        marker = 0;         // clear the found marker - this is a false positive
    } else {
#ifdef DEBUG_SPI_COMMUNICATION
        Debug::out(LOG_DEBUG, "readHeader - 1st 8 bytes: ");
        Debug::outBfr(rxBuffer, 8);
#endif
    }

    return marker;
}

// not waiting for anything, any received packet should be put in rxQueue
void CConSpi2::transferNow(void)
{
    waitForATN(WAIT_FOR_NOTHING, ATN_ANY, 0);
}