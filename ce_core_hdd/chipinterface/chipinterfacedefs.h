#ifndef __CHIPINTERFACEDEFS_H__
#define __CHIPINTERFACEDEFS_H__

#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>

// types of chip interface, as returned by the individual chip interface classes
#define CHIP_IF_DUMMY   -1
#define CHIP_IF_V1_V2   1
#define CHIP_IF_V3      3
#define CHIP_IF_V4      4
#define CHIP_IF_RASCSI  8
#define CHIP_IF_NETWORK 9

// The following commands are sent from device to host on chip interface v1 and v2,
// but as they are used for command identification in core thread and are reused
// in chip interface v3 (even though that one doesn't really use them), it's moved here.

#define ATN_FW_VERSION                  0x01        // followed by string with FW version (length: 4 WORDs - cmd, v[0], v[1], 0)
#define ATN_ACSI_COMMAND                0x02
#define ATN_READ_MORE_DATA              0x03
#define ATN_WRITE_MORE_DATA             0x04
#define ATN_GET_STATUS                  0x05
#define ATN_ANY                         0xff        // this is used only on host to wait for any ATN

// defines for Floppy part
// commands sent from device to host
#define ATN_FW_VERSION              0x01            // followed by string with FW version (length: 4 WORDs - cmd, v[0], v[1], 0)
#define ATN_SEND_NEXT_SECTOR        0x02            // sent: 2, side, track #, current sector #, 0, 0, 0, 0 (length: 4 WORDs)
#define ATN_SECTOR_WRITTEN          0x03            // sent: 3, side (highest bit) + track #, current sector #
#define ATN_SEND_TRACK              0x04            // send the whole track

// commands sent from Franz v4 to host
#define ATN_GET_DISPLAY_DATA        0x05            // get current display content

#define COMMAND_SIZE            10
#define ACSI_CMD_SIZE           14
#define WRITTENMFMSECTOR_SIZE   2048
#define MFM_STREAM_SIZE         13800
#define TX_RX_BUFF_SIZE         600

// Hans: commands sent from host to device
#define CMD_ACSI_CONFIG                 0x10
#define CMD_DATA_WRITE                  0x20
#define CMD_DATA_READ_WITH_STATUS       0x30
#define CMD_SEND_STATUS                 0x40
#define CMD_DATA_READ_WITHOUT_STATUS    0x50
#define CMD_FLOPPY_CONFIG               0x70
#define CMD_FLOPPY_SWITCH               0x80
#define CMD_GET_LICENSE                 0xa0
#define CMD_DO_UPDATE                   0xb0
#define CMD_DATA_MARKER                 0xda

// Franz: commands sent from host to device
#define CMD_WRITE_PROTECT_OFF       0x10
#define CMD_WRITE_PROTECT_ON        0x20
#define CMD_DISK_CHANGE_OFF         0x30
#define CMD_DISK_CHANGE_ON          0x40
#define CMD_SET_DRIVE_ID_0          0x70
#define CMD_SET_DRIVE_ID_1          0x80
#define CMD_DRIVE_ENABLED           0xa0
#define CMD_DRIVE_DISABLED          0xb0

// Franz v4: new commands sent from host to device, they will be ignored in older Franz
#define CMD_FRANZ_MODE_1            0xc0        // Franz in v1/v2 mode
#define CMD_FRANZ_MODE_4_SOUND_ON   0xc1        // Franz in v4 mode + do floppy seek sound
#define CMD_FRANZ_MODE_4_SOUND_OFF  0xc2        // Franz in v4 mode + don't make the floppy seek sound
#define CMD_FRANZ_MODE_4_POWER_OFF  0xc5        // turn off the power of this device

#define MAKEWORD(A, B)  ( (((uint16_t)A)<<8) | ((uint16_t)B) )

#define HDD_FW_RESPONSE_LEN     12
#define FDD_FW_RESPONSE_LEN     8

#define FW_RESPONSE_LEN_BIGGER  ((HDD_FW_RESPONSE_LEN > FDD_FW_RESPONSE_LEN) ? HDD_FW_RESPONSE_LEN : FDD_FW_RESPONSE_LEN)

#define INBUF_SIZE  (WRITTENMFMSECTOR_SIZE + 8)

#define SYNC_TAG_HDD    0xc050d1c5
#define SYNC_TAG_FDD    0xc0500fdd

#define MAX_CLIENTS     8

#define MFM_STREAM_SIZE             13800

// defines for Floppy part
// commands sent from device to host
#define ATN_FW_VERSION              0x01            // followed by string with FW version (length: 4 WORDs - cmd, v[0], v[1], 0)
#define ATN_SECTOR_WRITTEN          0x03            // sent: 3, side (highest bit) + track #, current sector #
#define ATN_SEND_TRACK              0x04            // send the whole track
#define ATN_SEND_WHOLE_IMAGE        0x05            // send the whole image
#define ATN_ANY                     0xff            // this is used only on host to wait for any ATN

// Franz: commands sent from host to device
#define CMD_WRITE_PROTECT_OFF       0x10
#define CMD_WRITE_PROTECT_ON        0x20
#define CMD_DISK_CHANGE_OFF         0x30
#define CMD_DISK_CHANGE_ON          0x40
#define CMD_SET_DRIVE_ID_0          0x70
#define CMD_SET_DRIVE_ID_1          0x80
#define CMD_DRIVE_ENABLED           0xa0
#define CMD_DRIVE_DISABLED          0xb0
#define CMD_FRANZ_SOUND_ON          0xc1        // do floppy seek sound
#define CMD_FRANZ_SOUND_OFF         0xc2        // don't make the floppy seek sound

#endif
