#ifndef GLOBAL_H
#define GLOBAL_H

#include <string>
#include <stdint.h>
#include <pthread.h>

#define FD_EMPTY    -1

#define SERVER_UDP_PORT             7200        // port where the CE discovery for reports from cores and for requests from devices
#define CLIENT_UDP_PORT             7201        // this is where the CE device will wait for discovery response

#define SERVER_TCP_PORT_HDD_FIRST   7300        // first port used by HDD core (next at +1, +2, ...)

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


#define VERSION_STRING          "CosmosEx v4.00 (by Jookie)"
#define VERSION_STRING_SHORT    "4.00"
#define DATE_STRING             "01/11/26"
                              // MM/DD/YY


#define DEVTYPE_OFF                 0
#define DEVTYPE_SD                  1
#define DEVTYPE_RAW                 2
#define DEVTYPE_TRANSLATED          3

// types of devices / modules we support
#define HOSTMOD_CONFIG              1
#define HOSTMOD_LINUX_TERMINAL      2
#define HOSTMOD_TRANSLATED_DISK     3
#define HOSTMOD_NETWORK_ADAPTER     4
#define HOSTMOD_FDD_SETUP           5
#define HOSTMOD_MEDIA_STREAMING     6

//////////////////////////////////////////////////////
// commands for HOSTMOD_TRANSLATED_DISK
#define TRAN_CMD_IDENTIFY           0
#define TRAN_CMD_GETDATETIME        1
#define TRAN_CMD_SENDSCREENCAST     2
#define TRAN_CMD_SCREENCASTPALETTE  3
#define TRAN_CMD_SCREENSHOT_CONFIG  4
// ...other commands are just function codes from gemdos.h


//////////////////////////////////////////////////////
// HDD interface types
#define HDD_IF_ACSI     1
#define HDD_IF_SCSI     2

#define SCSI_MACHINE_UNKNOWN    0
#define SCSI_MACHINE_TT         1
#define SCSI_MACHINE_FALCON     2

//////////////////////////////////////////////////////
// chip interface types
#define CHIPIF_UNKNOWN  -1
#define CHIPIF_DUMMY    0
#define CHIPIF_V1_V2    1       // Hans + CPLD, Franz, via SPI
#define CHIPIF_V3       3       // Hosts via SPI
#define CHIPIF_V4       4       // Franz via SPI, ACSI via GPIO
#define CHIPIF_RASCSI   8
#define CHIPIF_NETWORK  9

//////////////////////////////////////////////////////

typedef struct {
    int  version;               // returned from Hans: HW version (1 for HW from 2014, 2 for HW from 2015, 3 for HW from 2020)
    int  hddIface;              // returned from Hans: HDD interface type (ACSI or SCSI (added in 2015))
    int  scsiMachine;           // when HwHddIface is HDD_IF_SCSI, this specifies what machine (TT or Falcon) is using this device

    uint8_t hwSerial[13];          // contains HW serial number, if HW version is 3 and device is running for few seconds

    bool changed;               // true if the value has changes recently
} THwConfig;

extern int logLevel;

typedef struct {
    // volatile uint8_t insertSpecialFloppyImageId;

    volatile bool screenShotVblEnabled;
    volatile bool doScreenShot;
    volatile bool hddReloadRaw;
    volatile bool hddReloadTranslated;
} InterProcessEvents;

extern InterProcessEvents events;

class ImageStorage;

typedef struct {
    int linuxTermFd;
    int configFd;
    int downloaderFd;
} ExternalServices;

extern ExternalServices externalServices;

//////////////////////////////////////////////////////

void preloadGlobalsFromDotEnv(void);

#define SPECIAL_FDD_IMAGE_CE_CONF       100
#define SPECIAL_FDD_IMAGE_FDD_TEST      101

#define CE_CONF_FDD_IMAGE_PATH_AND_FILENAME_TMP "/tmp/ce_conf.st"
#define CE_CONF_FDD_IMAGE_JUST_FILENAME         "ce_conf.st"

#define FDD_TEST_IMAGE_PATH_AND_FILENAME_TMP    "/tmp/fdd_test.st"
#define FDD_TEST_IMAGE_JUST_FILENAME            "fdd_test.st"

#define MAX_ZIPDIR_ZIPFILE_SIZE             (5*1024*1024)

#define PATH_ATARI_CE_FDD_TTP               "CE_FDD.TTP"
#define PATH_ATARI_CE_HDIMG_TTP             "CE_HDIMG.TTP"
#define PATH_ATARI_CE_MEDIAPLAY             "CEMEDIAP.TTP"

#define NETSERVER_WEBROOT                   "/tmp/ce_netserver_webroot"
#define NETSERVER_WEBROOT_INDEX             NETSERVER_WEBROOT "/index.html"

// These were global const string constants, but now they depend on .env content, so they are now
// loaded on app start and used when needed.
extern std::string corePath;
extern std::string CE_CONF_FDD_IMAGE_PATH_AND_FILENAME;
extern std::string FDD_TEST_IMAGE_PATH_AND_FILENAME;
extern std::string PATH_CE_DD_BS_L1;
extern std::string PATH_CE_DD_BS_L2;
extern std::string PATH_CE_DD_PRG_PATH_AND_FILENAME;
extern std::string CONFIG_DRIVE_PATH;

#define LOG_DIR_DEFAULT     "/tmp/ce/log"
#define DATA_DIR_DEFAULT    "/tmp/ce/data"
#define PID_DIR_DEFAULT     "/tmp/ce/pid"

#endif // GLOBAL_H
