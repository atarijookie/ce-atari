// vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab
#ifndef _PERIODICTHREAD_H_
#define _PERIODICTHREAD_H_

class AcsiDataTrans;
class Scsi;
class ConfigStream;
class ImageList;
class ImageStorage;
class ImageSilo;

typedef struct {
    int fd1;
    int fd2;
} ConfigPipes;

typedef struct {
    Scsi            *scsi;
    pthread_mutex_t mtxScsi;

    ImageList       *imageList;
    ImageStorage    *imageStorage;
    ImageSilo       *imageSilo;
    pthread_mutex_t  mtxImages;

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
} SharedObjects;

void *periodicThreadCode(void *ptr);

#endif
