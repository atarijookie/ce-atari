#ifndef _MAIN_H_
#define _MAIN_H_

// for usage of SW WRITE capturing have a SW_WRITE defined. HW WRITE doesn't work now, but code still kept for future tests / changes / improvements
//#define SW_WRITE

void setupAtnBuffers(void);

void fillMfmTimesForDMA(void);
BYTE getNextMFMbyte(void);

void getMfmWriteTimes(void);
void getMfmWriteTimesTemp(void);

void processHostCommand(BYTE val);
void requestTrack(void);

void spiDma_txRx(WORD txCount, WORD *txBfr, WORD rxCount, WORD *rxBfr);
BYTE spiDma_waitForFinish(void);
BYTE waitForSPIidle(void);

BYTE timeout(void);

#define ATN_SYNC_WORD                        0xcafe

// commands sent from device to host
#define ATN_FW_VERSION          0x01        // followed by string with FW version (length: 4 WORDs - cmd, v[0], v[1], 0)
#define ATN_SECTOR_WRITTEN      0x03        // sent: 3, side (highest bit) + track #, current sector #
#define ATN_SEND_TRACK          0x04        // send the whole track

// commands sent from host to device
#define CMD_WRITE_PROTECT_OFF       0x10
#define CMD_WRITE_PROTECT_ON        0x20
#define CMD_DISK_CHANGE_OFF         0x30
#define CMD_DISK_CHANGE_ON          0x40
#define CMD_CURRENT_SECTOR          0x50                            // followed by sector #
#define CMD_GET_FW_VERSION          0x60
#define CMD_SET_DRIVE_ID_0          0x70
#define CMD_SET_DRIVE_ID_1          0x80
#define CMD_CURRENT_TRACK           0x90                            // followed by track #
#define CMD_DRIVE_ENABLED           0xa0
#define CMD_DRIVE_DISABLED          0xb0
#define CMD_MARK_READ               0xF000                          // this is not sent from host, but just a mark that this WORD has been read and you shouldn't continue to read further
#define CMD_MARK_READ_BYTE          0xF0                            // this is not sent from host, but just a mark that this BYTE has been read and you shouldn't continue to read further
#define CMD_TRACK_STREAM_END        0xF000                          // this is the mark in the track stream that we shouldn't go any further in the stream
#define CMD_TRACK_STREAM_END_BYTE   0xF0                            // this is the mark in the track stream that we shouldn't go any further in the stream

#define MFM_4US     1
#define MFM_6US     2
#define MFM_8US     3

//-----------------------------------------
// lengths of buffers on SPI TX and RX
#define READTRACKDATA_SIZE_BYTES    13800
#define READTRACKDATA_SIZE_WORDS    (READTRACKDATA_SIZE_BYTES / 2)  // read track size - convert byte count to word count

#define ATN_SENDFWVERSION_LEN_TX    8
#define ATN_SENDFWVERSION_LEN_RX    8
#define CMD_BUFFER_SIZE             ATN_SENDFWVERSION_LEN_RX        // CMD buffer is RXed when Tx/Rx SENDFWVERSION, this is for convenience
// if you ever need the CMD buffer to be bigger, change it in ANT_SENDFEVERSION_LEN_RX value - to transfer the new size through SPI

#define ATN_SENDTRACK_REQ_LEN_TX    6
#define ATN_SENDTRACK_REQ_LEN_RX    READTRACKDATA_SIZE_WORDS
//-----------------------------------------

#define STREAM_TABLE_ITEMS  20
#define STREAM_TABLE_SIZE   STREAM_TABLE_ITEMS

#define STREAM_TABLE_OFFSET (10/2)              // 10 bytes / 5 words - the stream table starts at this offset, because first 5 words are empty (ATN + sizes + other)
#define STREAM_START_OFFSET (STREAM_TABLE_OFFSET + STREAM_TABLE_SIZE)

#endif
