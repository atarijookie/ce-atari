#ifndef CONSPI2_H
#define CONSPI2_H

#include <queue>
#include <list>
#include <stdint.h>
#include "../utils.h"
#include "../debug.h"
#include "../chipinterface.h"

#define WAIT_FOR_NOTHING        0       // not waiting for anything, any received packet should be put in rxQueue
#define WAIT_FOR_HDD            1       // waiting for HDD marker only
#define WAIT_FOR_FDD            2       // waiting for FDD marker only
#define WAIT_FOR_HDD_OR_FDD     3       // waiting for HDD or FDD marker

// v3 uses single physical stream, thus markers for each logical stream are different.
// They are all different from v1/v2 marker, so the v1/v2 chip interface code will fail with v3 protocol.
#define MARKER_HDD      0xfdca      // cafd - hdD
#define MARKER_FDD      0xffca      // caff - Fdd
#define MARKER_IKBD     0xfbca      // cafb - ikBd
#define MARKER_DISP     0xf5ca      // caf5 - di5play

// commands for display stream
#define CMD_DISPLAY     0x10        // display this buffer

// commands for IKBD stream
#define CMD_IKBD_DATA   0x10

// Horst chip in v3 uses different CS than Franz and Hans, because we want to have complete control over the CS line
#define PIN_CS_HORST    PIN_TX_SEL1N2

// This is the maximum size which CE want to TX or RX. It's based on the largest request size, which is ATN_SEND_TRACK.
#define SPI_TX_RX_BFR_SIZE 15000

// this is a dummy command sent to CE, when there is nothing actually to sent,
// but we need something to send actually receive data. CE can ignore the rest of the buffer after finding this cmd.
#define CMD_DUMMY 0x15

class SpiTxPacket
{
public:
    SpiTxPacket(uint16_t marker, uint16_t cmd, uint16_t dataSizeInBytes, uint8_t* inData)
    {
        Utils::storeWord(data + 0, 0);                  // pre-pad with zero
        Utils::storeWord(data + 2, marker);
        Utils::storeWord(data + 4, cmd);
        Utils::storeWord(data + 6, dataSizeInBytes);   // packet will hold the EXACT number of bytes we're sending

        if(dataSizeInBytes > 0 && inData != NULL) {    // if packet has some data
            memcpy(data + 8, inData, dataSizeInBytes); // copy in data
        }

        // the transfer size might be bigger by +1 if input data size is odd (but packet will hold exact number of real data)
        if((dataSizeInBytes & 1) != 0) {               // odd number of bytes? make it even, we're sending words...
            dataSizeInBytes++;
        }

        size = 8 + dataSizeInBytes + 2;                // packet size = header + data + trailing zero
    }

    uint8_t data[SPI_TX_RX_BFR_SIZE];
    uint16_t size;
};

class SpiRxPacket
{
public:
    SpiRxPacket(void) {
        pData = NULL;
        pDataDynamic = NULL;            // no data dynamically allocated yet
    };

    ~SpiRxPacket(void) {
        if(pDataDynamic) {              // if data was dynamically allocated, free it
            delete []pDataDynamic;
            pDataDynamic = NULL;
        }
    };

    void useRawData(uint8_t* inData, bool copyNotPoint)
    {
        pData = inData;                         // store pointer for usage
        uint16_t txSize = txLen();

        if(txSize > SPI_TX_RX_BFR_SIZE) {       // the TX size seems wrong?
            Debug::out(LOG_ERROR, "SpiRxPacket - TX size is %d bytes, but buffer is only %d bytes!", txSize, SPI_TX_RX_BFR_SIZE);
            return;
        }

        if(copyNotPoint) {                      // if should make a copy of data
            if(pDataDynamic == NULL) {          // we don't have this buffer yet, allocate
                pDataDynamic = new uint8_t[SPI_TX_RX_BFR_SIZE];
            }

            memcpy(pDataDynamic, inData, txSize);   // copy data
            pData = pDataDynamic;                   // store pointer to data
        }
    }

    // returns true if this packet is for HDD / FDD as requested, and the ATN code matches
    bool asRequested(uint8_t expectMarker, uint8_t atnCodeIn)
    {
        // from the expectMarker create flags which tell us is HDD and FDD are accepted here
        bool acceptsHdd = (expectMarker == WAIT_FOR_HDD_OR_FDD || expectMarker == WAIT_FOR_HDD);
        bool acceptsFdd = (expectMarker == WAIT_FOR_HDD_OR_FDD || expectMarker == WAIT_FOR_FDD);

        // if it's HDD when expecting HDD or and it's FDD when expecting FDD, the accepted markers match
        bool acceptedMarkersMatch = (acceptsHdd && isHdd()) || (acceptsFdd && isFdd());

        // not founc match between wanting HDD/FDD and getting HDD/FDD, then this packet is not as requested
        if(!acceptedMarkersMatch) {
            return false;
        }

        // HDD / FDD as requested and ANY ATN is acceptable, good
        if(atnCodeIn == ATN_ANY) {
            return true;
        }

        // HDD / FDD as requested, but ATN code is specific, so as requested if wanted ATN code equals this ATN code
        return atnCode() == atnCodeIn;
    }

    // data format: marker, ATN code, txLen, rxLen
    uint16_t marker(void)   { return Utils::getWord(pData    ); }
    uint16_t atnCode(void)  { return Utils::getWord(pData + 2); }

    uint16_t txLen(void)    {
        uint16_t inSize = Utils::getWord(pData + 4) * 2;    // multiply by 2 because size is in WORDs
        return MIN(inSize, SPI_TX_RX_BFR_SIZE);             // limit to maximum buffer size
    }

    bool isHdd(void)        { return marker() == MARKER_HDD; }
    bool isFdd(void)        { return marker() == MARKER_FDD; }

    uint8_t* getDataPointer(void)   { return (pData + 8); }                 // data starts after 8 bytes of header
    uint16_t getDataSize(void)      { return (txLen() - 8 - 2); }     // size of data = TX size - header size - trailing zero size

    uint8_t* getBaseDataPointer(void)   { return pData; }                   // returns pointer tho base of whole packet

private:
    uint8_t* pData;                     // store pointer to data
    uint8_t* pDataDynamic;
};


class CConSpi2
{
public:
    CConSpi2();
    ~CConSpi2();
    void setIkbdWriteFd(int fd) { ikbdWriteFd = fd; }
    SpiRxPacket* waitForATN(uint8_t expectMarker, uint8_t atnCode, uint32_t timeoutMs);
    void addToTxQueue(uint16_t marker, uint16_t cmd, uint16_t dataSizeInBytes, uint8_t* inData);
    void transferNow(void);

private:
    int ikbdWriteFd;
    pthread_mutex_t mutex;
    SpiTxPacket *txDummyPacket;

    // Add packets that need to be sent to CE device to this TX queue. This code does not need to examine the packets
    // any further after they are inserted here, so it's really done using a queue.
    std::queue<SpiTxPacket *> txQueue;

    // Received packets are placed in this RX queue. When a larger HDD transfer is in progress, some packets
    // not related to that HDD transfer might be received, they are placed in this queue while the larger HDD transfer
    // is in progress, and later they are processed. They are examined in the order they were received (like a queue),
    // but some of them might need to be skipped and processed later then others, so this queue is actually a list.
    std::list<SpiRxPacket *> rxQueue;

    uint8_t txBuffer[SPI_TX_RX_BFR_SIZE];
    uint8_t rxBuffer[SPI_TX_RX_BFR_SIZE];

    uint16_t readHeader(void);
    uint16_t swapWord(uint16_t val);

    void setCsForTransfer(bool startNotEnd);
    uint16_t transferPacket(void);

    void testEndian(void);
};

#endif // CONSPI2_H
