#include <cstring>

#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/spi.h"
#include "hardware/dma.h"

#include "defs.h"
#include "utils.h"
#include "serial_config.h"

// Write a buffer to PSRAM
void psramWriteBuffer(uint32_t addr, const uint8_t *buffer, size_t length)
{
    uint8_t cmd[4] = {0x02, (uint8_t) (addr >> 16), (uint8_t) (addr >> 8), (uint8_t) addr};      // write command

    gpio_put(PIN_CS, 0);
    spi_write_blocking(spi1, cmd, 4);
    spi_write_blocking(spi1, buffer, length);
    gpio_put(PIN_CS, 1);
}

// Read a buffer from PSRAM
void psramReadBuffer(uint32_t addr, uint8_t *buffer, size_t length)
{
    uint8_t cmd[4] = {0x03, (uint8_t) (addr >> 16), (uint8_t) (addr >> 8), (uint8_t) addr};      // read command

    gpio_put(PIN_CS, 0);
    spi_write_blocking(spi1, cmd, 4);
    spi_read_blocking(spi1, 0, buffer, length);
    gpio_put(PIN_CS, 1);
}

// Read ID of PSRAM
void psramReadId(void)
{
    uint8_t cmd[4] = {0x9f, 0, 0, 0};   // 'Read ID' command
    uint8_t resp[3];

    gpio_put(PIN_CS, 0);
    spi_write_blocking(spi1, cmd, 4);
    spi_read_blocking(spi1, 0, resp, 3);
    gpio_put(PIN_CS, 1);

    if(resp[0] != 0x0d) {
        xprintf("PSRAM init - bad PSRAM ID: %02x. Will fail to work! HALT!\n", resp[0]);
        while(1);
    }

    xprintf("PSRAM init - OK\n");
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

    uint32_t start = millis();
    psramReadBuffer(0, readBfr, TEST_BFR_SIZE);         // read from psram
    uint32_t end = millis();

    if(memcmp(writeBfr, readBfr, TEST_BFR_SIZE) != 0)   // write and read buffers mismatch? fail and halt
    {
        xprintf("PSRAM not working correctly! HALT\n");
        while(1);
    } 

    xprintf("PSRAM used and working, read takes %d ms\n", end - start);

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
