#ifndef _FLOPPYSETUP_H_
#define _FLOPPYSETUP_H_

#include <stdio.h>

#include "../settingsreloadproxy.h"
#include "../datatypes.h"
#include "../translated/translateddisk.h"
#include "imagesilo.h"
#include "imagelist.h"

#define IMG_DN_STATUS_IDLE          0
#define IMG_DN_STATUS_DOWNLOADING   1
#define IMG_DN_STATUS_DOWNLOADED    2

class AcsiDataTrans;

class FloppySetup
{
public:
    FloppySetup();
    ~FloppySetup();

    void processCommand(BYTE *command);

    void setAcsiDataTrans(AcsiDataTrans *dt);
    void setImageSilo(ImageSilo *imgSilo);
    void setTranslatedDisk(TranslatedDisk *td);

    static bool createNewImage(std::string pathAndFile);

private:
    AcsiDataTrans       *dataTrans;
    ImageSilo           *imageSilo;
    TranslatedDisk      *translated;
    ImageList           imageList;    

    FILE    *up;
    BYTE    *cmd;

    BYTE    bfr64k[64 * 1024 + 4];

    int         imgDnStatus;
    std::string inetDnFilePath;
    std::string inetDnFilename;

    struct {
        int         slotIndex;
        std::string atariSourcePath;
        std::string hostSourcePath;
        std::string hostDestinationPath;
        std::string file;
        FILE *fh;
    } currentUpload;

    struct {
        std::string hostDestPath;
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
    void searchMark(void);
    void searchDownload(void);
    void searchRefreshList(void);
};

#endif

