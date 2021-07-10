#ifndef GLOBAL_H
#define GLOBAL_H

#include "datatypes.h"

// commands sent from host to device
#define CMD_CURRENT_SECTOR          0x50                                // followed by sector #
#define CMD_GET_FW_VERSION          0x60
#define CMD_CURRENT_TRACK           0x90                                // followed by track #
#define CMD_MARK_READ               0xF000                              // this is not sent from host, but just a mark that this WORD has been read and you shouldn't continue to read further

#define MFM_4US     1
#define MFM_6US     2
#define MFM_8US     3


#define VERSION_STRING          "CosmosEx v3.00 (by Jookie)"
#define VERSION_STRING_SHORT    "3.00"
#define DATE_STRING             "05/09/21"
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
#define HOSTMOD_MISC                7

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
#define CHIPIF_UNKNOWN  0
#define CHIPIF_V1_V2    1       // Hans + CPLD, Franz, via SPI
#define CHIPIF_NETWORK  2

//////////////////////////////////////////////////////

typedef struct {
    char serial  [20];
    char revision[8];
    char model   [40];

    DWORD revisionInt;
} RPiConfig;

typedef struct {
    int  version;               // returned from Hans: HW version (1 for HW from 2014, 2 for HW from 2015, 3 for HW from 2020)
    int  hddIface;              // returned from Hans: HDD interface type (ACSI or SCSI (added in 2015))
    int  scsiMachine;           // when HwHddIface is HDD_IF_SCSI, this specifies what machine (TT or Falcon) is using this device
    bool fwMismatch;            // when HW and FW types don't match (e.g. SCSI HW + ACSI FW, or ACSI HW + SCSI FW)

    BYTE hwSerial[13];          // contains HW serial number, if HW version is 3 and device is running for few seconds
    bool hwLicenseValid;        // if true, then HW has a valid license and we don't need to show HW lincese screen in config

    bool changed;               // true if the value has changes recently
} THwConfig;

typedef struct {
    bool justShowHelp;          // show possible command line arguments and quit
    int  logLevel;              // init current log level to LOG_ERROR
    int  chipInterface;         // what kind of chip interface we will use - CHIPIF_...
    bool justDoReset;           // if shouldn't run the app, but just reset Hans and Franz (used with STM32 ST-Link JTAG)
    bool noReset;               // don't reset Hans and Franz on start - used with STM32 ST-Link JTAG
    bool test;                  // if set to true, set ACSI ID 0 to translated, ACSI ID 1 to SD, and load floppy with some image
    bool actAsCeConf;           // if set to true, this app will behave as ce_conf app instead of CosmosEx app
    bool getHwInfo;             // if set to true, wait for HW info from Hans, and then quit and report it
    bool noFranz;               // if set to true, won't communicate with Franz
    bool ikbdLogs;              // if set to true, will generate ikbd logs file
    bool fakeOldApp;            // if set to true, will always return old app version, so you can test app installation over and over
    bool display;               // if set to true, show string on front display, if possible

    bool deviceGetLicense;      // if true, device should get license again
    bool deviceDoUpdate;        // if true, device should download update and write it to flash

    bool gotHansFwVersion;
    bool gotFranzFwVersion;
} TFlags;

typedef struct {
    volatile BYTE insertSpecialFloppyImageId;

    volatile bool screenShotVblEnabled;
    volatile bool doScreenShot;
} InterProcessEvents;

extern InterProcessEvents events;

//////////////////////////////////////////////////////

#define SPECIAL_FDD_IMAGE_CE_CONF       100
#define SPECIAL_FDD_IMAGE_FDD_TEST      101

#define CE_CONF_FDD_IMAGE_PATH_AND_FILENAME     "/ce/app/ce_conf.st"
#define CE_CONF_FDD_IMAGE_PATH_AND_FILENAME_TMP "/tmp/ce_conf.st"
#define CE_CONF_FDD_IMAGE_JUST_FILENAME         "ce_conf.st"

#define FDD_TEST_IMAGE_PATH_AND_FILENAME        "/ce/app/fdd_test.st"
#define FDD_TEST_IMAGE_PATH_AND_FILENAME_TMP    "/tmp/fdd_test.st"
#define FDD_TEST_IMAGE_JUST_FILENAME            "fdd_test.st"

#define CE_CONF_TAR_PATH_AND_FILENAME       "/ce/app/ce_conf.zip"
#define CE_CONF_TAR_JUST_FILENAME           "ce_conf.zip"

#define MAX_ZIPDIR_ZIPFILE_SIZE             (5*1024*1024)
#define MAX_ZIPDIR_NESTING                  3

#define PATH_CE_DD_BS_L1                    "/ce/app/configdrive/drivers/ce_dd.bs"
#define PATH_CE_DD_BS_L2                    "/ce/app/configdrive/drivers/ce_dd_l2.bs"

#define PATH_CE_DD_PRG_PATH_AND_FILENAME    "/ce/app/configdrive/drivers/ce_dd.prg"
#define PATH_CE_DD_PRG_JUST_FILENAME        "ce_dd.prg"

#define PATH_CE_CONF_PRG_PATH_AND_FILENAME  "/ce/app/configdrive/ce_conf.tos"
#define PATH_CE_CONF_PRG_JUST_FILENAME      "ce_conf.tos"

#define PATH_ATARI_CE_FDD_TTP               "CE_FDD.TTP"
#define PATH_ATARI_CE_HDIMG_TTP             "CE_HDIMG.TTP"
#define PATH_ATARI_CE_MEDIAPLAY             "CEMEDIAP.TTP"

#define NETSERVER_WEBROOT                   "/tmp/ce_netserver_webroot"
#define NETSERVER_WEBROOT_INDEX             NETSERVER_WEBROOT "/index.html"

#endif // GLOBAL_H

