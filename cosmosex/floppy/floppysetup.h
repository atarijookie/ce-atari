#ifndef _FLOPPYSETUP_H_
#define _FLOPPYSETUP_H_

#include <stdio.h>

#include "../settingsreloadproxy.h"
#include "../datatypes.h"
#include "../translated/translateddisk.h"
#include "imagesilo.h"

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

private:
    AcsiDataTrans       *dataTrans;
    ImageSilo           *imageSilo;
    TranslatedDisk      *translated;

    FILE    *up;
    BYTE    *cmd;

    BYTE    bfr64k[64 * 1024 + 4];

    struct {
        int         slotIndex;
        std::string atariSourcePath;
        std::string hostSourcePath;
        std::string hostDestinationPath;
        std::string file;
        FILE *fh;
    } currentUpload;

    void uploadStart(void);
    void uploadBlock(void);
    void uploadEnd(void);
};

#endif

