// vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab
#ifndef MEDIASTREAMING_H
#define MEDIASTREAMING_H

#include "datatypes.h"

class TranslatedDisk;
class AcsiDataTrans;

#define MEDIASTREAMING_MAXSTREAMS    4

typedef struct {
    unsigned short audioRate;
    bool forceMono;
} MediaParams;

class MediaStream
{
public:
    MediaStream(void);
    ~MediaStream();
    bool isFree(void);
    bool open(const char * filename, const MediaParams * params);
    int getInfos(BYTE * buffer, int bufferlen);
    int read(BYTE * buffer, int bufferlen);
    void close(void);
private:
    FILE * f;
    bool isPipe;
};

class MediaStreaming
{
private:
    MediaStreaming(void);
    ~MediaStreaming();
    static MediaStreaming * instance;
public:
    static MediaStreaming * getInstance(void);
    static void deleteInstance(void);

    void processCommand(BYTE *command, AcsiDataTrans *dataTrans);

private:
    void openStream(AcsiDataTrans *dataTrans);
    void getStreamInfo(BYTE streamHandle, AcsiDataTrans *dataTrans);
    void readStream(BYTE arg, AcsiDataTrans *dataTrans);
    void closeStream(BYTE streamHandle, AcsiDataTrans *dataTrans);

// properties
    MediaStream streams[MEDIASTREAMING_MAXSTREAMS];
};

#endif // MEDIASTREAMING_H
