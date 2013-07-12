#ifndef GLOBAL_H
#define GLOBAL_H

// commands sent from device to host
#define ATN_FW_VERSION              0x01       		// followed by string with FW version (length: 4 WORDs - cmd, v[0], v[1], 0)
#define ATN_SEND_NEXT_SECTOR        0x02               	// sent: 2, side, track #, current sector #, 0, 0, 0, 0 (length: 4 WORDs)
#define ATN_SECTOR_WRITTEN          0x03               	// sent: 3, side (highest bit) + track #, current sector #

// commands sent from host to device
#define CMD_WRITE_PROTECT_OFF		0x10
#define CMD_WRITE_PROTECT_ON		0x20
#define CMD_DISK_CHANGE_OFF			0x30
#define CMD_DISK_CHANGE_ON			0x40
#define CMD_CURRENT_SECTOR			0x50								// followed by sector #
#define CMD_GET_FW_VERSION			0x60
#define CMD_SET_DRIVE_ID_0			0x70
#define CMD_SET_DRIVE_ID_1			0x80
#define CMD_MARK_READ				0xF000                              // this is not sent from host, but just a mark that this WORD has been read and you shouldn't continue to read further

#define MFM_4US     1
#define MFM_6US     2
#define MFM_8US     3

extern "C" void outDebugString(const char *format, ...);

#endif // GLOBAL_H
