#ifndef _FLOPPYSETUP_H_
#define _FLOPPYSETUP_H_

#include <stdio.h>

#include "../settingsreloadproxy.h"
#include <stdint.h>
#include "../translated/translateddisk.h"
#include "../settingsreloadproxy.h"

#define FLOPPY_UPLOAD_PATH          "/tmp/"

#define IMG_DN_STATUS_IDLE          0
#define IMG_DN_STATUS_DOWNLOADING   1
#define IMG_DN_STATUS_DOWNLOADED    2

// these are used to define if download and insert buttons are visible in download app
#define ROW_OBJ_HIDDEN      0
#define ROW_OBJ_VISIBLE     1
#define ROW_OBJ_SELECTED    2

class AcsiDataTrans;

class FloppySetup
{
public:
    FloppySetup();
    ~FloppySetup();

    void processCommand(uint8_t *command);

    void setAcsiDataTrans(AcsiDataTrans *dt);
    void setSettingsReloadProxy(SettingsReloadProxy *rp);

    static bool createNewImage(std::string pathAndFile);

private:
    int                 screenResolution;
    AcsiDataTrans       *dataTrans;
    SettingsReloadProxy *reloadProxy;

    FILE    *up;
    uint8_t    *cmd;

    uint8_t    bfr64k[64 * 1024 + 4];

    //int         imgDnStatus;
    std::string inetDnFilePath;
    std::string inetDnFilename;

    struct {
        int         slotIndex;
        std::string atariSourcePath;
        std::string hostPath;
        std::string file;
        FILE *fh;
    } currentUpload;

    struct {
        std::string hostPath;
        std::string imageFile;
        FILE *fh;
    } currentDownload;

    void uploadStart(void);
    void uploadBlock(void);
    void uploadEnd(bool isOnDeviceCopy);

    void newImage(void);
    void getNewImageName(char *nameBfr);

    void downloadStart(void);
    void downloadGetBlock(void);
    void downloadDone(void);
    void downloadOnDevice(void);

    void searchInit(void);
    void searchString(void);
    void searchResult(void);
    void searchRefreshList(void);

//  void searchMark(void);              // OBSOLETE
//  void searchDownload(void);          // OBSOLETE

    void searchInsertToSlot(void);

    void getCurrentSlot(void);
    void setCurrentSlot(void);
    void getImageEncodingRunning(void);

    void logCmdName(uint8_t cmdCode);
};

#endif


