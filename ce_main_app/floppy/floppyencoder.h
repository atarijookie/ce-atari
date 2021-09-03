// vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab
#ifndef _FLOPPYENCODER_H_
#define _FLOPPYENCODER_H_

#include <pthread.h>

#include <string>
#include <queue>

#include <stdint.h>
#include "../settingsreloadproxy.h"

#include "floppyimagefactory.h"
#include "mfmdecoder.h"
#include "mfmcachedimage.h"

#define WRITTENMFMSECTOR_COUNT  128
#define WRITTENMFMSECTOR_SIZE   2048

typedef struct {
    bool hasData;

    int slotNo;

    int track;
    int side;
    int sector;

    uint8_t *data;
    uint32_t size;
} WrittenMfmSector;

void *floppyEncodeThreadCode(void *ptr);

void floppyEncoder_stop(void);
void floppyEncoder_addEncodeWholeImageRequest(int slotNo, const char *imageFileName);
void floppyEncoder_addReencodeTrackRequest(int track, int side);
void floppyEncoder_decodeMfmWrittenSector(int track, int side, int sector, uint8_t *data, uint32_t size);

#endif
