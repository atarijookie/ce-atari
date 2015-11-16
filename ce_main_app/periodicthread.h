#ifndef _PERIODICTHREAD_H_
#define _PERIODICTHREAD_H_

#include "native/translatedbootmedia.h"
#include "native/scsi.h"
#include "translated/translateddisk.h"

typedef struct {
    Scsi            *scsi;

    TranslatedDisk  *translated;

} SharedObjects;

void *periodicThreadCode(void *ptr);

#endif

