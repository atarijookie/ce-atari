#ifndef __DEFS_H__
#define __DEFS_H__

#define MFM_4US         1
#define MFM_6US         2
#define MFM_8US         3

#define PULSE_TOO_SHORT 18
#define PULSE_4US       40
#define PULSE_6US       57
#define PULSE_8US       72

#define PIN_WGATE       (1 << 3)        // GPIOA 3, write is happening when WGATE is L
#define PIN_RXE         (1 << 5)        // GPIOA 5, SPI can get more data if this is H

#define TAG_WRITE_START 0x80    // start of sector data
#define TAG_WRITE_END   0xc0    // end of sector data

#define STATE_EMPTY         0       // buffer currently not used and is empty
#define STATE_STORING       1       // write data is being stored here, but it's still incomplete, and doesn't have tags, so will be ignored by esp32
#define STATE_WAIT_FOR_SEND 2       // all data stored, start and stop tags present, but waiting for DMA to send it via SPI
#define STATE_SENDING       3       // DMA is currently sending this part of buffer

#define MFM_READ_SIZE       32
#define MFM_READ_SIZE_HALF  (MFM_READ_SIZE / 2)
#define MFM_READ_SIZE_FILLS (MFM_READ_SIZE_HALF / 4)

#define MFM_WRITE_SIZE      64
#define MFM_WRITE_SIZE_HALF (MFM_WRITE_SIZE/2)

#define CIRC_HANDLE_NOTHING 0
#define CIRC_HANDLE_LOW     1
#define CIRC_HANDLE_HIGH    2

extern volatile uint8_t circHandleWhat;

extern uint16_t mfmReadStreamBuffer[MFM_READ_SIZE];
extern uint16_t mfmWriteStreamBuffer[MFM_WRITE_SIZE];

extern volatile uint8_t txDataState1, txDataState2;

/*
 * With 512 bytes buffer we can stream almost 8 ms without refilling,
 * this should give host enough time to handle other stuff.
 *
 * Transfer in 256 bytes blocks.
 * RXE pin will indicate if at least 256 bytes can be RXed.
 */

#define BFR_SIZE            512
#define BFR_SIZE_HALF       (BFR_SIZE / 2)
#define BFR_MASK            0x1FF
#define BFR_SIZE_CAN_RX     BFR_SIZE_HALF

/*
 * Maximum bytes with mfm symbols count in written sector == write buffer size
 * - you get the most symbols in sector, if all data bytes are zeros or all ones
 * - if you write header + data: 1189 bytes (you write this only when formatting?)
 * - if you write just data part: 1096 bytes (you write this when you write new sector data)
 * This means that 1200 bytes should be enough for written data.
 */
#define WRITEBUFFER_SIZE    1200

extern uint32_t txCnt;
extern uint8_t txData1[WRITEBUFFER_SIZE], txData2[WRITEBUFFER_SIZE];

extern volatile uint32_t rxCnt;
extern uint32_t rxLoad;
extern uint8_t rxData[BFR_SIZE];

extern volatile uint8_t writingNow;

extern uint8_t* pWrite;

extern uint8_t wrStreamByte;
extern uint8_t wrBits;

extern volatile uint16_t wrPrevCapturedStamp;
#endif
