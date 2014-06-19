#ifndef _SDMEDIA_H
#define _SDMEDIA_H

#include <stdio.h>
#include "../datatypes.h"
#include "imedia.h"

class SdMedia: public IMedia
{
public:
    SdMedia();
    virtual ~SdMedia();

	void setCurrentCapacity(DWORD sectors);
	
    virtual bool iopen(char *path, bool createIfNotExists);
    virtual void iclose(void);

    virtual bool isInit(void);
    virtual bool mediaChanged(void);
    virtual void setMediaChanged(bool changed);
    virtual void getCapacity(DWORD &bytes, DWORD &sectors);

    virtual bool readSectors(DWORD sectorNo, DWORD count, BYTE *bfr);
    virtual bool writeSectors(DWORD sectorNo, DWORD count, BYTE *bfr);

private:
	DWORD	capacityInSectors;
	bool	mediaChangedFlag;
	BYTE	senseKey;
	
};

#endif // _SDMEDIA_H
