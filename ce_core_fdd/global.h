#ifndef GLOBAL_H
#define GLOBAL_H

#include <string>
#include <stdint.h>
#include <pthread.h>

#define FD_EMPTY    -1

#define SERVER_TCP_PORT_FDD         7400        // port used by FDD core
#define SERVER_TCP_PORT_IKBD        7401        // port used by IKBD core

// commands sent from host to device
// #define CMD_WRITE_PROTECT_OFF    0x10
// #define CMD_WRITE_PROTECT_ON     0x20
// #define CMD_DISK_CHANGE_OFF      0x30
// #define CMD_DISK_CHANGE_ON       0x40
#define CMD_CURRENT_SECTOR          0x50        // followed by sector #
#define CMD_GET_FW_VERSION          0x60
// #define CMD_SET_DRIVE_ID_0       0x70
// #define CMD_SET_DRIVE_ID_1       0x80
#define CMD_CURRENT_TRACK           0x90        // followed by track #
// #define CMD_DRIVE_ENABLED        0xa0
// #define CMD_DRIVE_DISABLED       0xb0
#define CMD_DATA_PART_OF_SECTOR     0xC0

#define CMD_TRACK_STREAM_END        0xF0    // this is the mark in the track stream that we shouldn't go any further in the stream

#define ENCODED_SECTOR_MAX_SIZE     1200    // the mfm encoded sector - header + gaps + markers + data - should not exceed this size. Using fixed size to simplify sector write to memory in device.

#define MFM_4US     1
#define MFM_6US     2
#define MFM_8US     3

typedef struct {
    bool justShowHelp;          // show possible command line arguments and quit
    int  logLevel;              // init current log level to LOG_ERROR
} TFlags;

class ImageSilo;

typedef struct {
    ImageSilo       *imageSilo;
    pthread_mutex_t  mtxImages;
} SharedObjects;

extern SharedObjects shared;

//////////////////////////////////////////////////////

void preloadGlobalsFromDotEnv(void);

// These were global const string constants, but now they depend on .env content, so they are now
// loaded on app start and used when needed.
extern std::string CONFIG_DRIVE_PATH;

#define LOG_DIR_DEFAULT     "/tmp/ce/log"
#define DATA_DIR_DEFAULT    "/tmp/ce/data"
#define PID_DIR_DEFAULT     "/tmp/ce/pid"

#endif // GLOBAL_H
