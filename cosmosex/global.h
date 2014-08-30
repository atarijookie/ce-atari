#ifndef GLOBAL_H
#define GLOBAL_H

// defines for Floppy part
// commands sent from device to host
#define ATN_FW_VERSION              0x01       		// followed by string with FW version (length: 4 WORDs - cmd, v[0], v[1], 0)
#define ATN_SEND_NEXT_SECTOR        0x02            // sent: 2, side, track #, current sector #, 0, 0, 0, 0 (length: 4 WORDs)
#define ATN_SECTOR_WRITTEN          0x03            // sent: 3, side (highest bit) + track #, current sector #
#define ATN_SEND_TRACK              0x04            // send the whole track

// commands sent from host to device
#define CMD_WRITE_PROTECT_OFF		0x10
#define CMD_WRITE_PROTECT_ON		0x20
#define CMD_DISK_CHANGE_OFF			0x30
#define CMD_DISK_CHANGE_ON			0x40
#define CMD_CURRENT_SECTOR			0x50								// followed by sector #
#define CMD_GET_FW_VERSION			0x60
#define CMD_SET_DRIVE_ID_0			0x70
#define CMD_SET_DRIVE_ID_1			0x80
#define CMD_CURRENT_TRACK           0x90                                // followed by track #
#define CMD_DRIVE_ENABLED           0xa0
#define CMD_DRIVE_DISABLED          0xb0
#define CMD_MARK_READ				0xF000                              // this is not sent from host, but just a mark that this WORD has been read and you shouldn't continue to read further

#define MFM_4US     1
#define MFM_6US     2
#define MFM_8US     3


#define VERSION_STRING          "CosmosEx v1.00 (by Jookie)"
#define VERSION_STRING_SHORT    "1.00"
#define DATE_STRING             "10/09/13"
                                // MM/DD/YY


#define DEVTYPE_OFF					0
#define DEVTYPE_SD                  1
#define DEVTYPE_RAW					2
#define DEVTYPE_TRANSLATED			3

// typed of devices / modules we support
#define HOSTMOD_CONFIG				1
#define HOSTMOD_LINUX_TERMINAL		2
#define HOSTMOD_TRANSLATED_DISK		3
#define HOSTMOD_NETWORK_ADAPTER		4
#define HOSTMOD_FDD_SETUP           5

//////////////////////////////////////////////////////
// commands for HOSTMOD_CONFIG
#define CFG_CMD_IDENTIFY            0
#define CFG_CMD_KEYDOWN             1
#define CFG_CMD_SET_RESOLUTION      2
#define CFG_CMD_REFRESH             0xfe
#define CFG_CMD_GO_HOME             0xff


//////////////////////////////////////////////////////
// commands for HOSTMOD_TRANSLATED_DISK
#define TRAN_CMD_IDENTIFY           0
#define TRAN_CMD_GETDATETIME        1
#define TRAN_CMD_SENDSCREENCAST     2
#define TRAN_CMD_SCREENCASTPALETTE  3
// ...other commands are just function codes from gemdos.h


//////////////////////////////////////////////////////
// commands for HOSTMOD_FDD_SETUP
#define FDD_CMD_IDENTIFY                    0
#define FDD_CMD_GETSILOCONTENT              1

#define FDD_CMD_UPLOADIMGBLOCK_START        10
#define FDD_CMD_UPLOADIMGBLOCK_FULL         11
#define FDD_CMD_UPLOADIMGBLOCK_PART         12
#define FDD_CMD_UPLOADIMGBLOCK_DONE_OK      13
#define FDD_CMD_UPLOADIMGBLOCK_DONE_FAIL    14

#define FDD_CMD_SWAPSLOTS                   20
#define FDD_CMD_REMOVESLOT                  21
#define FDD_CMD_NEW_EMPTYIMAGE              22
#define FDD_CMD_GET_CURRENT_SLOT            25
#define FDD_CMD_SET_CURRENT_SLOT            26
#define FDD_CMD_GET_IMAGE_ENCODING_RUNNING  28

#define FDD_CMD_DOWNLOADIMG_START           30
#define FDD_CMD_DOWNLOADIMG_GETBLOCK        31
#define FDD_CMD_DOWNLOADIMG_DONE            32
#define FDD_CMD_DOWNLOADIMG_ONDEVICE        35

#define FDD_CMD_SEARCH_INIT                 40      // call this to init the image search
#define FDD_CMD_SEARCH_STRING               41      // search for a string, create vector of results
#define FDD_CMD_SEARCH_RESULTS              42      // retrieve one page of results
#define FDD_CMD_SEARCH_MARK                 43      // mark one image for download
#define FDD_CMD_SEARCH_DOWNLOAD             45      // start / check the download process
#define FDD_CMD_SEARCH_REFRESHLIST          48      // delete old image list, download a new one

#define FDD_OK                              0
#define FDD_ERROR                           2
#define FDD_DN_LIST                         4
#define FDD_DN_WORKING                      5
#define FDD_DN_DONE                         6
#define FDD_DN_NOTHING_MORE                 7
#define FDD_RES_ONDEVICECOPY                0xDC

//////////////////////////////////////////////////////

#endif // GLOBAL_H

