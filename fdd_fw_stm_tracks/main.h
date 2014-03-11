#ifndef _MAIN_H_
#define _MAIN_H_

void setupAtnBuffers(void);

void fillMfmTimesForDMA(void);
BYTE getNextMFMbyte(void);

void getMfmWriteTimes(void);
void getMfmWriteTimesTemp(void);

void processHostCommand(BYTE val);
void requestTrack(void);

void spiDma_txRx(WORD txCount, BYTE *txBfr, WORD rxCount, BYTE *rxBfr);
void spiDma_waitForFinish(void);
void waitForSPIidle(void);

BYTE timeout(void);

typedef struct {
	BYTE side;
	BYTE track;
	BYTE sector;
} SStreamed;

#define ATN_SYNC_WORD						0xcafe

// commands sent from device to host
#define ATN_FW_VERSION					0x01								// followed by string with FW version (length: 4 WORDs - cmd, v[0], v[1], 0)
#define ATN_SECTOR_WRITTEN      0x03               	// sent: 3, side (highest bit) + track #, current sector #
#define ATN_SEND_TRACK          0x04            		// send the whole track

// commands sent from host to device
#define CMD_WRITE_PROTECT_OFF			0x10
#define CMD_WRITE_PROTECT_ON			0x20
#define CMD_DISK_CHANGE_OFF				0x30
#define CMD_DISK_CHANGE_ON				0x40
#define CMD_CURRENT_SECTOR				0x50								// followed by sector #
#define CMD_GET_FW_VERSION				0x60
#define CMD_SET_DRIVE_ID_0				0x70
#define CMD_SET_DRIVE_ID_1				0x80
#define CMD_CURRENT_TRACK       	0x90                // followed by track #
#define CMD_MARK_READ							0xF000							// this is not sent from host, but just a mark that this WORD has been read and you shouldn't continue to read further
#define CMD_MARK_READ_BYTE				0xF0								// this is not sent from host, but just a mark that this BYTE has been read and you shouldn't continue to read further
#define CMD_TRACK_STREAM_END			0xF000							// this is the mark in the track stream that we shouldn't go any further in the stream
#define CMD_TRACK_STREAM_END_BYTE	0xF0								// this is the mark in the track stream that we shouldn't go any further in the stream

#define MFM_4US     1
#define MFM_6US     2
#define MFM_8US     3

//-----------------------------------------
// lengths of buffers on SPI TX and RX
// read track data buffer is in BYTEs, but REQ_LEN_RX is in WORDs
#define READTRACKDATA_SIZE					15000

#define ATN_SENDFWVERSION_LEN_TX		8
#define ATN_SENDFWVERSION_LEN_RX		8

#define ATN_SENDTRACK_REQ_LEN_TX		6
#define ATN_SENDTRACK_REQ_LEN_RX		(READTRACKDATA_SIZE / 2)
//-----------------------------------------

#endif
