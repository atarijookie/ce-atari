#ifndef __TRANSLATEDBOOTMEDIA_H_
#define __TRANSLATEDBOOTMEDIA_H_

#include <stdio.h>
#include "../datatypes.h"
#include "imedia.h"

#define TRANSLATEDBOOTMEDIA_SIZE	(32 * 1024)

class TranslatedBootMedia: public IMedia
{
public:
    TranslatedBootMedia();
    virtual ~TranslatedBootMedia();

    virtual bool iopen(char *path, bool createIfNotExists);
    virtual void iclose(void);

    virtual bool isInit(void);
    virtual bool mediaChanged(void);
    virtual void setMediaChanged(bool changed);
    virtual void getCapacity(int64_t &bytes, int64_t &sectors);

    virtual bool readSectors(int64_t sectorNo, DWORD count, BYTE *bfr);
    virtual bool writeSectors(int64_t sectorNo, DWORD count, BYTE *bfr);

	void updateBootsectorConfigWithACSIid(BYTE acsiId);
	
private:

    int64_t	BCapacity;			// device capacity in bytes
    int64_t	SCapacity;			// device capacity in sectors

	bool	gotImage;
    BYTE	*imageBuffer;
	
	bool	loadDataIntoBuffer(void);
	void 	updateBootsectorConfig(void);
	int		getConfigPosition(void);
	void	setDword(BYTE *bfr, DWORD val);
	
	void	updateBootsectorChecksum(void);
    void    calculateChecksum(BYTE *bfr);
	WORD	swapNibbles(WORD val);
};

#endif // __TRANSLATEDBOOTMEDIA_H_

