#ifndef GLOBAL_H
#define GLOBAL_H

#include <string>
#include <stdint.h>
#include <pthread.h>

#define FD_EMPTY    -1

#define SERVER_TCP_PORT_FDD         7400        // port used by FDD core
#define SERVER_TCP_PORT_IKBD        7401        // port used by IKBD core

// commands sent from host to device
#define CMD_CURRENT_SECTOR          0x50                                // followed by sector #
#define CMD_GET_FW_VERSION          0x60
#define CMD_CURRENT_TRACK           0x90                                // followed by track #
#define CMD_MARK_READ               0xF000                              // this is not sent from host, but just a mark that this uint16_t has been read and you shouldn't continue to read further

#define MFM_4US     1
#define MFM_6US     2
#define MFM_8US     3

typedef struct {
    bool justShowHelp;          // show possible command line arguments and quit
    int  logLevel;              // init current log level to LOG_ERROR
    int  portClient;    
} TFlags;

typedef struct {
    volatile uint8_t insertSpecialFloppyImageId;
} InterProcessEvents;

extern InterProcessEvents events;

class ImageStorage;
class ImageSilo;

typedef struct {
    ImageStorage    *imageStorage;
    ImageSilo       *imageSilo;
    pthread_mutex_t  mtxImages;
} SharedObjects;

extern SharedObjects shared;

//////////////////////////////////////////////////////

void preloadGlobalsFromDotEnv(void);

#define SPECIAL_FDD_IMAGE_CE_CONF       100
#define SPECIAL_FDD_IMAGE_FDD_TEST      101

#define CE_CONF_FDD_IMAGE_PATH_AND_FILENAME_TMP "/tmp/ce_conf.st"
#define CE_CONF_FDD_IMAGE_JUST_FILENAME         "ce_conf.st"

#define FDD_TEST_IMAGE_PATH_AND_FILENAME_TMP    "/tmp/fdd_test.st"
#define FDD_TEST_IMAGE_JUST_FILENAME            "fdd_test.st"

// These were global const string constants, but now they depend on .env content, so they are now
// loaded on app start and used when needed.
extern std::string corePath;
extern std::string CE_CONF_FDD_IMAGE_PATH_AND_FILENAME;
extern std::string FDD_TEST_IMAGE_PATH_AND_FILENAME;
extern std::string CONFIG_DRIVE_PATH;

#define LOG_DIR_DEFAULT     "/tmp/ce/log"
#define DATA_DIR_DEFAULT    "/tmp/ce/data"
#define PID_DIR_DEFAULT     "/tmp/ce/pid"

#endif // GLOBAL_H
