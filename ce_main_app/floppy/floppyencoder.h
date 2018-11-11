// vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab
#ifndef _FLOPPYENCODER_H_
#define _FLOPPYENCODER_H_

#include <pthread.h>

#include <string>
#include <queue>

#include "../datatypes.h"
#include "../settingsreloadproxy.h"

#include "floppyimagefactory.h"
#include "mfmdecoder.h"
#include "mfmcachedimage.h"

void *floppyEncodeThreadCode(void *ptr);

void floppyEncoder_stop(void);
void floppyEncoder_addEncodeWholeImageRequest(int slotNo, const char *imageFileName);
void floppyEncoder_addReencodeTrackRequest(int slotNo, int track, int side);

#endif
