// vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab
#ifndef _IMAGESILO_H_
#define _IMAGESILO_H_

#include <pthread.h>

#include <string>
#include <queue>

#include <stdint.h>
#include "../chipinterface_network/chipinterfacenetwork.h"

#include "mfmdecoder.h"
#include "mfmcachedimage.h"

#define SLOT_COUNT          MAX_CLIENTS

#define EMPTY_IMAGE_PATH    "/tmp/emptyimage.st"

typedef struct
{
    int slotNo;                     // slot number of this slot (used for debugging)

    std::string     imageFile;      // just file name:                     bla.st
    std::string     imageFileNoExt; // file name without extension:        bla
    std::string     hostPath;       // for translated disk, host path:     /mnt/sda/gamez/bla.st
                                    // or where uploaded disk where is stored: /tmp/bla.st

    volatile bool   openRequested;  // set to true when doing open file request, set to false after successful open
    volatile uint32_t  openRequestTime; // timestamp when other thread requested opening of image
    volatile uint32_t  openActionTime; // timestamp when encoder did really open the file
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

    uint8_t *getEncodedTrack(int floppySlotIndex, int track, int side, int &bytesInBuffer);
    bool getParams(int floppySlotIndex, int &tracks, int &sides, int &sectorsPerTrack);
    std::string getFileName(int floppySlotIndex);
    uint8_t *getEmptyTrack(void);

    void add(int positionIndex, std::string &filename, std::string &hostPath);
    void remove(int index);

    void dumpStringsToBuffer(uint8_t *bfr);

    bool createNewImage(std::string pathAndFile);

private:
    void clearSlot(int index);

    uint8_t *emptyTrack;
};

#endif
