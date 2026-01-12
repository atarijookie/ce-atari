#ifndef __TRANSLATEDBOOTMEDIA_H_
#define __TRANSLATEDBOOTMEDIA_H_

#include <stdio.h>
#include <stdint.h>
#include "imedia.h"

#define TRANSLATEDBOOTMEDIA_SIZE    (128 * 1024)

class TranslatedBootMedia: public IMedia
{
public:
    TranslatedBootMedia();
    virtual ~TranslatedBootMedia();

    virtual bool iopen(const char *path, bool createIfNotExists);
    virtual void iclose(void);

    virtual bool isInit(void);
    virtual bool mediaChanged(void);
    virtual void setMediaChanged(bool changed);
    virtual void getCapacity(int64_t &bytes, int64_t &sectors);

    virtual bool readSectors (int64_t sectorNo, uint32_t count, uint8_t *bfr);
    virtual bool writeSectors(int64_t sectorNo, uint32_t count, uint8_t *bfr);

    void updateBootsectorConfigWithACSIid(uint8_t acsiId);

private:
    int     hwHddIfaceCurrent;  // this is the hwHddIface for which the bootsector was loaded, reload on mismatch

    int64_t BCapacity;          // device capacity in bytes
    int64_t SCapacity;          // device capacity in sectors

    bool    gotImage;
    uint8_t    *imageBuffer;

    uint8_t    lastUsedAcsiId;

    bool    loadDataIntoBuffer(void);
    void    updateBootsectorConfig(void);
    int     getConfigPosition(void);
    void    setDword(uint8_t *bfr, uint32_t val);

    void    updateBootsectorChecksum(void);
    void    calculateChecksum(uint8_t *bfr);
    uint16_t    swapNibbles(uint16_t val);
};

#endif // __TRANSLATEDBOOTMEDIA_H_

