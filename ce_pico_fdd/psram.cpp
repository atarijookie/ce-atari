#include <cstring>

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/dma.h"

#include "defs.h"
#include "utils.h"
#include "serial_config.h"

void spiTxAsync(const uint8_t* bfr, size_t length)
{
    // SPI.transferAsync(bfr, nullptr, length);
    // while (!SPI.finishedAsync());
}

void spiRxAsync(uint8_t* bfr, size_t length)
{
    // SPI.transferAsync(nullptr, bfr, length);
    // while (!SPI.finishedAsync());
}

// Write a buffer to PSRAM
void psramWriteBuffer(uint32_t addr, const uint8_t *buffer, size_t length)
{
    // SPI.beginTransaction(spisettings);
    gpio_put(PIN_CS, 0);

    uint8_t cmd[4];
    cmd[0] = 0x02;                  // Write command
    cmd[1] = (addr >> 16) & 0xFF;
    cmd[2] = (addr >>  8) & 0xFF;
    cmd[3] = (addr      ) & 0xFF;

    spiTxAsync(cmd, 4);
    spiTxAsync(buffer, length);

    gpio_put(PIN_CS, 1);
    // SPI.endTransaction();
}

// Read a buffer from PSRAM
void psramReadBuffer(uint32_t addr, uint8_t *buffer, size_t length)
{
    // SPI.beginTransaction(spisettings);
    gpio_put(PIN_CS, 0);

    uint8_t cmd[4];
    cmd[0] = 0x03;                  // read command
    cmd[1] = (addr >> 16) & 0xFF;
    cmd[2] = (addr >>  8) & 0xFF;
    cmd[3] = (addr      ) & 0xFF;

    spiTxAsync(cmd, 4);
    spiRxAsync(buffer, length);

    gpio_put(PIN_CS, 1);
    // SPI.endTransaction();
}

// Read ID of PSRAM
void psramReadId(void)
{
    // SPI.beginTransaction(spisettings);
    gpio_put(PIN_CS, 0);

    uint8_t cmd[4];
    cmd[0] = 0x9f;                  // 'Read ID' command
    cmd[1] = 0;
    cmd[2] = 0;
    cmd[3] = 0;

    spiTxAsync(cmd, 4);

    uint8_t resp[3];
    spiRxAsync(resp, 3);

    gpio_put(PIN_CS, 1);
    // SPI.endTransaction();

    if(resp[0] != 0x0d) {
        printf("PSRAM init - bad PSRAM ID: %02x. Will fail to work! HALT!\n", resp[0]);
        while(1);
    }

    printf("PSRAM init - OK\n");
}

void psramTest(void)
{
    psramReadId();

    #define TEST_BFR_SIZE   13800

    uint8_t writeBfr[TEST_BFR_SIZE];
    for(int i=0; i<TEST_BFR_SIZE; i++) {                // full buffer with random data
        writeBfr[i] = random();
    }

    psramWriteBuffer(0, writeBfr, TEST_BFR_SIZE);       // write to psram

    uint8_t readBfr[TEST_BFR_SIZE];
    memset(readBfr, 0, TEST_BFR_SIZE);                  // clear the read buffer
    psramReadBuffer(0, readBfr, TEST_BFR_SIZE);         // read from psram

    if(memcmp(writeBfr, readBfr, TEST_BFR_SIZE) != 0)   // write and read buffers mismatch? fail and halt
    {
        printf("PSRAM not working correctly! HALT\n");
        while(1);
    } 

    printf("PSRAM used and working\n");

    // clear the psram block used in test
    memset(writeBfr, 0, TEST_BFR_SIZE);
    psramWriteBuffer(0, writeBfr, TEST_BFR_SIZE);
}

void psramStoreTrack(int track, int side, uint8_t* data)
{
    side = (side == 0) ? 0 : 1;     // limit side to values 0 and 1
    track = MIN(track, MAX_TRACKS); // limit track to MAX_TRACKS
    uint32_t address = ((track * 2) + side) * READTRACKDATA_SIZE_BYTES;

    psramWriteBuffer(address, data, READTRACKDATA_SIZE_BYTES);
}

void psramLoadTrack(int track, int side, uint8_t* data)
{
    side = (side == 0) ? 0 : 1;     // limit side to values 0 and 1
    track = MIN(track, MAX_TRACKS); // limit track to MAX_TRACKS
    uint32_t address = ((track * 2) + side) * READTRACKDATA_SIZE_BYTES;

    psramReadBuffer(address, data, READTRACKDATA_SIZE_BYTES);
}

void psramStoreSector(int track, int side, int byteOffsetFromTrackStart, uint8_t* data, uint32_t copyLength, uint32_t clearLength)
{
    side = (side == 0) ? 0 : 1;     // limit side to values 0 and 1
    track = MIN(track, MAX_TRACKS); // limit track to MAX_TRACKS
    uint32_t address = ((track * 2) + side) * READTRACKDATA_SIZE_BYTES;
    address += byteOffsetFromTrackStart;    // we will start writing from the specified sector offset

    if(copyLength > 0) {
        psramWriteBuffer(address, data, copyLength);
    }

    if(clearLength > 0) {
        uint8_t clearBfr[ENCODED_SECTOR_MAX_SIZE];
        memset(clearBfr, 0, clearLength);

        address += copyLength;                  // address will now point beyond written sector data
        psramWriteBuffer(address, clearBfr, clearLength);
    }
}

void psramConfigFlagSet(void)
{
    uint8_t bfr[4];
    storeDword(bfr, RUN_CONFIG_AFTER_RESTART);
    psramWriteBuffer(0, bfr, 4);
}

void psramConfigFlagClear(void)
{
    uint8_t bfr[4];
    memset(bfr, 0, 4);
    psramWriteBuffer(0, bfr, 4);
}

bool psramConfigFlagGet(void)
{
    uint8_t bfr[4];
    psramReadBuffer(0, bfr, 4);
    uint32_t flag = getDword(bfr);

    return (flag == RUN_CONFIG_AFTER_RESTART);
}
