// vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab
#ifndef _IMAGESILO_H_
#define _IMAGESILO_H_

#include <pthread.h>

#include <string>
#include <queue>

#include "../datatypes.h"
#include "../settingsreloadproxy.h"

#include "floppyimagefactory.h"
#include "mfmdecoder.h"
#include "mfmcachedimage.h"

#define EMPTY_IMAGE_SLOT        3
#define EMPTY_IMAGE_PATH        "/tmp/emptyimage.st"

//-------------------------------------------
// these globals here are just for status report
typedef struct
{
    std::string imageFile;
} SiloSlotSimple;
//-------------------------------------------

typedef struct
{
    std::string     imageFile;      // just file name:                     bla.st
    std::string     hostDestPath;   // where the file is stored when used: /tmp/bla.st
    std::string     atariSrcPath;   // from where the file was uploaded:   C:\gamez\bla.st
    std::string     hostSrcPath;    // for translated disk, host path:     /mnt/sda/gamez/bla.st

    MfmCachedImage  encImage;
} SiloSlot;

typedef struct
{
    int                slotIndex;                // number of slot for which this is done
    std::string        filename;                // file name and path where the image is located
    MfmCachedImage    *encImg;                // pointer to where this image should be stored after encoding
} EncodeRequest;

void *floppyEncodeThreadCode(void *ptr);

class ImageSilo
{
public:
    ImageSilo();
    ~ImageSilo();

    static void run(void);
    static void stop(void);

    void loadSettings(void);
    void saveSettings(void);
    void setSettingsReloadProxy(SettingsReloadProxy *rp);

    BYTE getSlotBitmap(void);
    void setCurrentSlot(int index);
    int  getCurrentSlot(void);
    BYTE *getEncodedTrack(int track, int side, int &bytesInBuffer);
    bool getParams(int &tracks, int &sides, int &sectorsPerTrack);
    BYTE *getEmptyTrack(void);

    void add(int positionIndex, std::string &filename, std::string &hostDestPath, std::string &atariSrcPath, std::string &hostSrcPath, bool saveToSettings);
    void swap(int index);
    void remove(int index);

    bool containsImage(const char *filename);
    bool currentSlotHasNewContent(void);

    void dumpStringsToBuffer(BYTE *bfr);

    SiloSlot *getSiloSlot(int index);

    static int getFloppyImageSelectedId(void);
    static SiloSlotSimple * getFloppyImageSimple(int index);
    static bool getFloppyEncodingRunning(void);

private:
    void clearSlot(int index);
    static void addEncodeRequest(EncodeRequest &er);

    SiloSlot                slots[4];
    int                        currentSlot;
    SettingsReloadProxy     *reloadProxy;

    BYTE                    *emptyTrack;

    static pthread_mutex_t floppyEncodeQueueMutex;
    static pthread_cond_t floppyEncodeQueueNotEmpty;
    static std::queue<EncodeRequest> encodeQueue;
    static volatile bool shouldStop;

    static volatile bool floppyEncodingRunning;
    static SiloSlotSimple floppyImages[3];
    static int floppyImageSelected;
};

#endif
