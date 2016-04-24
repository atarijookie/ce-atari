#ifndef MEDIASTREAMING_H
#define MEDIASTREAMING_H

#include <stdio.h>
#include "acsidatatrans.h"

#define MEDIASTREAMING_MAXSTREAMS	4

class MediaStream
{
public:
	MediaStream(void);
	bool isFree(void);
	bool open(const char * filename);
	int getInfos(BYTE * buffer, int bufferlen);
	int read(BYTE * buffer, int bufferlen);
	void close(void);
private:
	FILE * f;
};

class MediaStreaming
{
public:
	MediaStreaming(void);
	//virtual ~MediaStreaming();

	void processCommand(BYTE *command, AcsiDataTrans *dataTrans);
private:
	void openStream(AcsiDataTrans *dataTrans);
	void getStreamInfo(BYTE streamHandle, AcsiDataTrans *dataTrans);
	void readStream(BYTE arg, AcsiDataTrans *dataTrans);

// properties
	MediaStream streams[MEDIASTREAMING_MAXSTREAMS];
};

#endif // MEDIASTREAMING_H
