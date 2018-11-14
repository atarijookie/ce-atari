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

#define SLOT_COUNT          4

#define EMPTY_IMAGE_SLOT    3
#define EMPTY_IMAGE_PATH    "/tmp/emptyimage.st"

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
    std::string     imageFileNoExt; // file name without extension:        bla
    std::string     atariSrcPath;   // from where the file was uploaded:   C:\gamez\bla.st
    std::string     hostPath;       // for translated disk, host path:     /mnt/sda/gamez/bla.st
                                    // or where uploaded disk where is stored: /tmp/bla.st

    volatile DWORD  openRequestTime; // timestamp when other thread requested opening of image
    volatile DWORD  openActionTime; // timestamp when encoder did really open the file
    std::string     imageFileName;  // file name of image to open next
    FloppyImage     *image;         // this holds object with the loaded floppy image (in normal data form)

    MfmCachedImage  encImage;       // this holds the MFM encoded image ready to be streamed
} SiloSlot;

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

    void add(int positionIndex, std::string &filename, std::string &hostPath, std::string &atariSrcPath, bool saveToSettings);
    void swap(int index);
    void remove(int index);

    bool containsImage(const char *filename);
    void containsImageInSlots(std::string &filenameWExt, std::string &bfr);
    void removeByFileName(std::string &filenameWExt);
    bool currentSlotHasNewContent(void);

    void dumpStringsToBuffer(BYTE *bfr);

    SiloSlot *getSiloSlot(int index);

    static int getFloppyImageSelectedId(void);
    static SiloSlotSimple * getFloppyImageSimple(int index);
    static bool getFloppyEncodingRunning(void);

private:
    void clearSlot(int index);

    SettingsReloadProxy     *reloadProxy;

    BYTE                    *emptyTrack;

    static SiloSlotSimple floppyImages[3];
    static int floppyImageSelected;
};

#endif
