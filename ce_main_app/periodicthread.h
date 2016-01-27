#ifndef _PERIODICTHREAD_H_
#define _PERIODICTHREAD_H_

#include "native/translatedbootmedia.h"
#include "native/scsi.h"
#include "translated/translateddisk.h"
#include "acsidatatrans.h"

#include "config/configstream.h"
#include "acsidatatrans.h"

typedef struct {
    int fd1;
    int fd2;
} ConfigPipes;

typedef struct {
    Scsi            *scsi;
    pthread_mutex_t mtxScsi;
    
    TranslatedDisk  *translated;
    pthread_mutex_t mtxTranslated;

    struct {
        ConfigStream    *acsi;
        ConfigStream    *web;
        ConfigStream    *term;
        
        AcsiDataTrans   *dataTransWeb;
        AcsiDataTrans   *dataTransTerm;
    } configStream;
    
    struct {
        ConfigPipes web;
        ConfigPipes term;
    } configPipes;
    
    pthread_mutex_t mtxConfigStreams;
    
    bool mountRawNotTrans;
    
    bool devFinder_detachAndLook;
    bool devFinder_look;
    
} SharedObjects;

void *periodicThreadCode(void *ptr);

#endif

