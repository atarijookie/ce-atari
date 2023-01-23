#include <stdio.h>
#include <string.h>

#include "translatedbootmedia.h"
#include "../debug.h"
#include "../utils.h"
#include "../global.h"

extern THwConfig hwConfig;

TranslatedBootMedia::TranslatedBootMedia()
{
    BCapacity       = TRANSLATEDBOOTMEDIA_SIZE;
    SCapacity       = TRANSLATEDBOOTMEDIA_SIZE / 512;

    imageBuffer     = new uint8_t[TRANSLATEDBOOTMEDIA_SIZE];       // allocate and clean the buffer for boot image
    memset(imageBuffer, 0, TRANSLATEDBOOTMEDIA_SIZE);

    gotImage        = false;                                    // mark that we don't have the image yet
    lastUsedAcsiId  = 0xff;

    bool res = loadDataIntoBuffer();                            // try to load the data from disk to buffer

    if(res) {                                                   // if succeeded, mark that we got the image now
        gotImage = true;
    }
}

TranslatedBootMedia::~TranslatedBootMedia()
{
    iclose();
    delete []imageBuffer;
}

bool TranslatedBootMedia::loadDataIntoBuffer(void)
{
    // read the bootsector into buffer
    size_t bytesRead;
    FILE *f;

    //-------------
    // load Level 1 bootsector
    f = fopen(PATH_CE_DD_BS_L1.c_str(), "rb");

    if(!f) {
        Debug::out(LOG_ERROR, "TranslatedBootMedia - failed to open Level 1 bootsector file: %s", PATH_CE_DD_BS_L1.c_str());
        return false;
    }
    Debug::out(LOG_DEBUG, "TranslatedBootMedia - loaded Level 1 bootsector file: %s", PATH_CE_DD_BS_L1.c_str());

    bytesRead = fread(&imageBuffer[0], 1, 512, f);

    if(bytesRead != 512) {
        Debug::out(LOG_ERROR, "TranslatedBootMedia - didn't read 512 bytes from Level 1 bootsector file: %s", PATH_CE_DD_BS_L1.c_str());
        return false;
    }

    fclose(f);

    hwHddIfaceCurrent = hwConfig.hddIface;     // store for which HDD IF it was prepared
    //-------------
    // load Level 2 bootsector
    f = fopen(PATH_CE_DD_BS_L2.c_str(), "rb");

    if(!f) {
        Debug::out(LOG_ERROR, "TranslatedBootMedia - failed to open Level 2 bootsector file: %s", PATH_CE_DD_BS_L2.c_str());
        return false;
    }

    bytesRead = fread(&imageBuffer[512], 1, 512, f);

    if(bytesRead != 512) {
        Debug::out(LOG_ERROR, "TranslatedBootMedia - didn't read 512 bytes from Level 2 bootsector file: %s", PATH_CE_DD_BS_L2.c_str());
        return false;
    }

    fclose(f);
    //-------------
    // read the CosmosEx driver into buffer
    f = fopen(PATH_CE_DD_PRG_PATH_AND_FILENAME.c_str(), "rb");

    if(!f) {
        Debug::out(LOG_ERROR, "TranslatedBootMedia - failed to open %s", PATH_CE_DD_PRG_PATH_AND_FILENAME.c_str());
        return false;
    }

    bytesRead   = 1024;
    int offset  = 1024;
    while(1) {
        if(feof(f)) {                                           // end of file? break
            break;
        }

        memset(&imageBuffer[offset], 0, 512);                   // clear buffer

#ifdef SAFEBOOT
        fread(&imageBuffer[offset], 1, 510, f);                 // read
        calculateChecksum(&imageBuffer[offset]);                // calculate checksum
#else
        fread(&imageBuffer[offset], 1, 512, f);                 // read
#endif
        offset += 512;                                          // move to next part

        bytesRead += 512;                                       // increment bytes read variable
    }

    fclose(f);

    // calculate the count of sectors the driver takes in buffer + 1 for boot sector + 1 for the last part of ce_dd.prg
    SCapacity       = bytesRead / 512;
    BCapacity       = SCapacity * 512;

    updateBootsectorConfig();                           // now set up read sector count and malloc size in boot sector config

    return true;
}

void TranslatedBootMedia::updateBootsectorConfig(void)
{
    uint32_t tsize, dsize, bsize, totalSize;

    int driverOffset = 1024;

    tsize = Utils::getDword(imageBuffer + driverOffset + 2);        // get size of text
    dsize = Utils::getDword(imageBuffer + driverOffset + 6);        // get size of data
    bsize = Utils::getDword(imageBuffer + driverOffset + 10);       // get size of bss

    totalSize = tsize + dsize + bsize;                  // total size of driver in RAM = size of text + data + bss
    totalSize = ((totalSize / 1024) + 2) * 1024;        // round the size to the nearest biggest kB

    int pos = getConfigPosition();

    if(pos == -1) {
        Debug::out(LOG_ERROR, "TranslatedBootMedia - didn't find the config position in bootsector.");
        return;
    }

    imageBuffer[pos + 7] = SCapacity - 1;               // store how many sectors we should read (without boot sector)
    setDword(&imageBuffer[pos + 2], totalSize);         // store how many RAM we need to reserve for driver in ST

    updateBootsectorChecksum();                         // update the checksum at the end

    Debug::out(LOG_DEBUG, "TranslatedBootMedia - bootsector will read %d sectors, the driver will take %d kB of RAM.", (int) imageBuffer[pos + 7], (int) (totalSize / 1024));

    updateBootsectorConfigWithACSIid(lastUsedAcsiId);
}

void TranslatedBootMedia::updateBootsectorChecksum(void)
{
    calculateChecksum(imageBuffer);
}

void TranslatedBootMedia::calculateChecksum(uint8_t *bfr)
{
    uint16_t sum = 0, val;
    uint16_t *p = (uint16_t *) bfr;

    for(int i=0; i<255; i++) {
        val = *p;
        val = swapNibbles(val);
        sum += val;
        p++;
    }

    uint16_t cs = 0x1234 - sum;
    sum = sum & 0xffff;

    bfr[510] = cs >> 8;         // store the check sum
    bfr[511] = cs;
}

uint16_t TranslatedBootMedia::swapNibbles(uint16_t val)
{
    uint16_t a,b;

    a = val >> 8;           // get upper
    b = val &  0xff;        // get lower

    return ((b << 8) | a);
}

void TranslatedBootMedia::updateBootsectorConfigWithACSIid(uint8_t acsiId)
{
    lastUsedAcsiId = acsiId;            // store this ACSI ID for future usage

    int pos = getConfigPosition();

    if(pos == -1) {
        Debug::out(LOG_ERROR, "TranslatedBootMedia - didn't find the config position in bootsector.");
        return;
    }

    uint8_t id;
    if(hwConfig.hddIface == HDD_IF_ACSI) {     // for ACSI - it's the ID (0 .. 7)
        id = acsiId;

        Debug::out(LOG_DEBUG, "TranslatedBootMedia::updateBootsectorConfigWithACSIid() - hddIface is ACSI, bootsector ID set to: %d", (int) id);
    } else {                            // for SCSI - it's 8 ... 15 (XBIOS 42 DMAread, see http://toshyp.atari.org/en/00400d.html#DMAread)
        id = (acsiId+8);

        Debug::out(LOG_DEBUG, "TranslatedBootMedia::updateBootsectorConfigWithACSIid() - hddIface is SCSI, bootsector ID set to: %d", (int) id);
    }

    imageBuffer[pos + 6] = id;          // store from which ID we should read the driver sectors

    updateBootsectorChecksum();         // update the checksum at the end
}

int TranslatedBootMedia::getConfigPosition(void)
{
    for(int i=0; i<512; i++) {                          // find the config position in bootsector
        if(imageBuffer[i] == 'X' && imageBuffer[i+1] == 'X') {
            return i;
        }
    }

    return -1;
}

void TranslatedBootMedia::setDword(uint8_t *bfr, uint32_t val)
{
    bfr[0] = (uint8_t) (val >> 24);       // get hi
    bfr[1] = (uint8_t) (val >> 16);       // get mid hi
    bfr[2] = (uint8_t) (val >>  8);       // get mid lo
    bfr[3] = (uint8_t) (val      );       // get lo
}

bool TranslatedBootMedia::iopen(const char *path, bool createIfNotExists)
{
    return gotImage;                        // don't do anything, all the things have been probably done already
}

void TranslatedBootMedia::iclose(void)
{
    // nothing to do
}

bool TranslatedBootMedia::isInit(void)
{
    return gotImage;                        // if got image, then it's init
}

bool TranslatedBootMedia::mediaChanged(void)
{
    return false;                           // this media never changes
}

void TranslatedBootMedia::setMediaChanged(bool changed)
{
    // nothing to do here
}

void TranslatedBootMedia::getCapacity(int64_t &bytes, int64_t &sectors)
{
    bytes   = BCapacity;
    sectors = SCapacity;
}

bool TranslatedBootMedia::readSectors(int64_t sectorNo, uint32_t count, uint8_t *bfr)
{
    if(!isInit()) {                             // if not initialized, failed
        return false;
    }

    if(sectorNo >= SCapacity) {                 // if trying to read sector beyond the last sector, fail
        return false;
    }

    if(sectorNo == 0) {                         // if loading boot sector
        if(hwHddIfaceCurrent != hwConfig.hddIface) {   // if HDD IF changed (e.g. it was first loaded when Franz didn't respond yet, and then Franz responded that it's SCSI)
            loadDataIntoBuffer();               // reload data from disk to virtual config drive image
        }
    }

    memset(bfr, 0, count * 512);                // clear the buffer

    uint32_t sectsRemaining = SCapacity - sectorNo;    // how many sectors we have left, if we start reading from position 'sectorNo'?

    if(count > sectsRemaining) {                // if trying to read more sectors than we have, fix this
        count = sectsRemaining;
    }

    uint32_t pos       = sectorNo * 512;           // convert sector # to offset in boot image buffer
    uint32_t byteCount = count * 512;

    memcpy(bfr, imageBuffer + pos, byteCount);  // copy in the requested bytes
    return true;
}

bool TranslatedBootMedia::writeSectors(int64_t sectorNo, uint32_t count, uint8_t *bfr)
{
    return false;                               // write not supported
}

