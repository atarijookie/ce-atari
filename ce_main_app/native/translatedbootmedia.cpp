#include <stdio.h>
#include <string.h>

#include "translatedbootmedia.h"
#include "../debug.h"
#include "../utils.h"
#include "../global.h"

extern int hwHddIface;      // returned from Hans: HDD interface type - HDD_IF_ACSI / HDD_IF_SCSI

TranslatedBootMedia::TranslatedBootMedia()
{
    BCapacity       = TRANSLATEDBOOTMEDIA_SIZE;
    SCapacity       = TRANSLATEDBOOTMEDIA_SIZE / 512;

	imageBuffer		= new BYTE[TRANSLATEDBOOTMEDIA_SIZE];		// allocate and clean the buffer for boot image
	memset(imageBuffer, 0, TRANSLATEDBOOTMEDIA_SIZE);	
	
	gotImage		= false;									// mark that we don't have the image yet
	
	bool res = loadDataIntoBuffer();							// try to load the data from disk to buffer
	
	if(res) {													// if succeeded, mark that we got the image now
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
    const char *bootsectorPath = NULL;
    
    if(hwHddIface == HDD_IF_ACSI) {     // for ACSI IF
        bootsectorPath = "/tmp/configdrive/ce_dd_st.bs";
    } else {                            // for SCSI IF
        bootsectorPath = "/tmp/configdrive/ce_dd_tt.bs";
    }
    f = fopen(bootsectorPath, "rb");

	if(!f) {
        Debug::out(LOG_ERROR, "TranslatedBootMedia - failed to open Level 1 bootsector file: %s", bootsectorPath);
		return false;
	}
    Debug::out(LOG_ERROR, "TranslatedBootMedia - loaded Level 1 bootsector file: %s", bootsectorPath);
	
	bytesRead = fread(&imageBuffer[0], 1, 512, f);
	
	if(bytesRead != 512) {
        Debug::out(LOG_ERROR, "TranslatedBootMedia - didn't read 512 bytes from Level 1 bootsector file: %s", bootsectorPath);
		return false;
	}
	
	fclose(f);
    
    hwHddIfaceCurrent = hwHddIface;     // store for which HDD IF it was prepared
	//-------------
    // load Level 2 bootsector
    f = fopen("/tmp/configdrive/ce_dd_l2.bs", "rb");

	if(!f) {
        Debug::out(LOG_ERROR, "TranslatedBootMedia - failed to open Level 2 bootsector file");
		return false;
	}
	
	bytesRead = fread(&imageBuffer[512], 1, 512, f);
	
	if(bytesRead != 512) {
        Debug::out(LOG_ERROR, "TranslatedBootMedia - didn't read 512 bytes from Level 2 bootsector file!");
		return false;
	}
	
	fclose(f);
	//-------------
	// read the CosmosEx driver into buffer
	f = fopen("/tmp/configdrive/ce_dd.prg", "rb");

	if(!f) {
        Debug::out(LOG_ERROR, "TranslatedBootMedia - failed to open ce_dd.prg!");
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

	updateBootsectorConfig();							// now set up read sector count and malloc size in boot sector config
	
	return true;
}

void TranslatedBootMedia::updateBootsectorConfig(void)
{
	DWORD tsize, dsize, bsize, totalSize;
	
    int driverOffset = 1024;
    
	tsize = Utils::getDword(imageBuffer + driverOffset + 2);        // get size of text
	dsize = Utils::getDword(imageBuffer + driverOffset + 6);        // get size of data
	bsize = Utils::getDword(imageBuffer + driverOffset + 10);       // get size of bss
	
	totalSize = tsize + dsize + bsize;					// total size of driver in RAM = size of text + data + bss
	totalSize = ((totalSize / 1024) + 2) * 1024;		// round the size to the nearest biggest kB 
	
	int pos = getConfigPosition();
	
	if(pos == -1) {
		Debug::out(LOG_ERROR, "TranslatedBootMedia - didn't find the config position in bootsector.");
		return;
	}
	
	imageBuffer[pos + 7] = SCapacity - 1;				// store how many sectors we should read (without boot sector)
	setDword(&imageBuffer[pos + 2], totalSize);			// store how many RAM we need to reserve for driver in ST
	
	updateBootsectorChecksum();							// update the checksum at the end
	
    Debug::out(LOG_INFO, "TranslatedBootMedia - bootsector will read %d sectors, the driver will take %d kB of RAM.", (int) imageBuffer[pos + 3], (int) (totalSize / 1024));
}

void TranslatedBootMedia::updateBootsectorChecksum(void)
{
    calculateChecksum(imageBuffer);
}

void TranslatedBootMedia::calculateChecksum(BYTE *bfr)
{
    WORD sum = 0, val;
    WORD *p = (WORD *) bfr;

    for(int i=0; i<255; i++) {
        val = *p;
        val = swapNibbles(val);
        sum += val;
        p++;
    }

    WORD cs = 0x1234 - sum;
    sum = sum & 0xffff;

    bfr[510] = cs >> 8;         // store the check sum
    bfr[511] = cs;
}

WORD TranslatedBootMedia::swapNibbles(WORD val)
{
    WORD a,b;

    a = val >> 8;           // get upper
    b = val &  0xff;        // get lower

    return ((b << 8) | a);
}
	
void TranslatedBootMedia::updateBootsectorConfigWithACSIid(BYTE acsiId)
{
	int pos = getConfigPosition();
	
	if(pos == -1) {
		Debug::out(LOG_ERROR, "TranslatedBootMedia - didn't find the config position in bootsector.");
		return;
	}
	
    BYTE id;
    if(hwHddIface == HDD_IF_ACSI) {     // for ACSI - it's the ID (0 .. 7)
        id = acsiId;
    } else {                            // for SCSI - it's the ID bit (0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80)
        id = (1 << acsiId);
    }
    
	imageBuffer[pos + 6] = id;          // store from which ID we should read the driver sectors
	
	updateBootsectorChecksum();         // update the checksum at the end
	
	Debug::out(LOG_INFO, "TranslatedBootMedia - bootsector config updated with new ACSI ID set to %d", (int) acsiId);
}

int TranslatedBootMedia::getConfigPosition(void)
{
	for(int i=0; i<512; i++) {							// find the config position in bootsector
		if(imageBuffer[i] == 'X' && imageBuffer[i+1] == 'X') {
			return i;
		}
	}

	return -1;
}

void TranslatedBootMedia::setDword(BYTE *bfr, DWORD val)
{
    bfr[0] = (BYTE) (val >> 24);       // get hi
    bfr[1] = (BYTE) (val >> 16);       // get mid hi
    bfr[2] = (BYTE) (val >>  8);       // get mid lo
    bfr[3] = (BYTE) (val      );       // get lo
}

bool TranslatedBootMedia::iopen(char *path, bool createIfNotExists)
{
    return gotImage;						// don't do anything, all the things have been probably done already
}

void TranslatedBootMedia::iclose(void)
{
	// nothing to do
}

bool TranslatedBootMedia::isInit(void)
{
    return gotImage;						// if got image, then it's init
}

bool TranslatedBootMedia::mediaChanged(void)
{
    return false;							// this media never changes
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

bool TranslatedBootMedia::readSectors(int64_t sectorNo, DWORD count, BYTE *bfr)
{
    if(!isInit()) {                             // if not initialized, failed
        return false;
    }

	if(sectorNo >= SCapacity) {					// if trying to read sector beyond the last sector, fail
		return false;
	}

    if(sectorNo == 0) {                         // if loading boot sector
        if(hwHddIfaceCurrent != hwHddIface) {   // if HDD IF changed (e.g. it was first loaded when Franz didn't respond yet, and then Franz responded that it's SCSI)
            loadDataIntoBuffer();               // reload data from disk to virtual config drive image
        }
    }
    
	memset(bfr, 0, count * 512);				// clear the buffer
	
	DWORD sectsRemaining = SCapacity - sectorNo;	// how many sectors we have left, if we start reading from position 'sectorNo'?
	
	if(count > sectsRemaining) {				// if trying to read more sectors than we have, fix this
		count = sectsRemaining;
	}
	
    DWORD pos		= sectorNo * 512;			// convert sector # to offset in boot image buffer
    DWORD byteCount	= count * 512;

	memcpy(bfr, imageBuffer + pos, byteCount);	// copy in the requested bytes
    return true;
}

bool TranslatedBootMedia::writeSectors(int64_t sectorNo, DWORD count, BYTE *bfr)
{
	return false;								// write not supported
}
