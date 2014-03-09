#include <stdio.h>
#include <string.h>

#include "translatedbootmedia.h"
#include "../debug.h"

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
	
	f = fopen("/ce/app/configdrive/cedd.bs", "rb");

	if(!f) {
        Debug::out("TranslatedBootMedia - failed to open bootsector file!");
		return false;
	}
	
	bytesRead = fread(&imageBuffer[0], 1, 512, f);
	
	if(bytesRead != 512) {
        Debug::out("TranslatedBootMedia - didn't read 512 bytes from bootsector file!");
		return false;
	}
	
	fclose(f);
	
	// read the CosmosEx driver into buffer
	f = fopen("/ce/app/configdrive/cedd.prg", "rb");

	if(!f) {
        Debug::out("TranslatedBootMedia - failed to open cedd.prg!");
		return false;
	}
	
	bytesRead = fread(&imageBuffer[512], 1, TRANSLATEDBOOTMEDIA_SIZE - 512, f);			// try to read data up to the size of buffer
	
	if(bytesRead < (5*1024)) {															// didn't read enough?
        Debug::out("TranslatedBootMedia - didn't read more than 5 kB of data from driver, this is probably wrong, failing preventively :)");
		return false;
	}

	if(!feof(f)) {
        Debug::out("TranslatedBootMedia - didn't reach the end of driver file when loading to RAM, buffer is probably too small, failing!");
		return false;
	}
	
	fclose(f);
	
	// calculate the count of sectors the driver takes in buffer + 1 for boot sector + 1 for the last part of cedd.prg
	SCapacity       = (bytesRead / 512) + 2;
    BCapacity       =  SCapacity * 512;

	updateBootsectorConfig();							// now set up read sector count and malloc size in boot sector config
	
	return true;
}

void TranslatedBootMedia::updateBootsectorConfig(void)
{
	DWORD tsize, dsize, bsize, totalSize;
	
	tsize = getDword(imageBuffer + 512 + 2);			// get size of text
	dsize = getDword(imageBuffer + 512 + 6);			// get size of data
	bsize = getDword(imageBuffer + 512 + 10);			// get size of bss
	
	totalSize = tsize + dsize + bsize;					// total size of driver in RAM = size of text + data + bss
	totalSize = ((totalSize / 1024) + 2) * 1024;		// round the size to the nearest biggest kB 
	
	int pos = getConfigPosition();
	
	if(pos == -1) {
		Debug::out("TranslatedBootMedia - didn't find the config position in bootsector.");
		return;
	}
	
	imageBuffer[pos + 3] = SCapacity - 1;				// store how many sectors we should read (without boot sector)
	setDword(&imageBuffer[pos + 4], totalSize);			// store how many RAM we need to reserve for driver in ST
	
	updateBootsectorChecksum();							// update the checksum at the end
	
    Debug::out("TranslatedBootMedia - bootsector will read %d sectors, the driver will take %d kB of RAM.", (int) imageBuffer[pos + 3], (int) (totalSize / 1024));
}

void TranslatedBootMedia::updateBootsectorChecksum(void)
{
    // create the check sum
    WORD sum = 0, val;
    WORD *p = (WORD *) imageBuffer;

    for(int i=0; i<255; i++) {
        val = *p;
        val = swapNibbles(val);
        sum += val;
        p++;
    }

    WORD cs = 0x1234 - sum;
    sum = sum & 0xffff;

    imageBuffer[510] = cs >> 8;         // store the check sum
    imageBuffer[511] = cs;
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
		Debug::out("TranslatedBootMedia - didn't find the config position in bootsector.");
		return;
	}
	
	imageBuffer[pos + 2] = acsiId;						// store from which ACSI ID we should read the driver sectors
	
	updateBootsectorChecksum();							// update the checksum at the end
	
	Debug::out("TranslatedBootMedia - bootsector config updated with new ACSI ID set to %d", (int) acsiId);
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

DWORD TranslatedBootMedia::getDword(BYTE *bfr)
{
    DWORD val = 0;

    val = bfr[0];       // get hi
    val = val << 8;

    val |= bfr[1];      // get mid hi
    val = val << 8;

    val |= bfr[2];      // get mid lo
    val = val << 8;

    val |= bfr[3];      // get lo

    return val;
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

void TranslatedBootMedia::getCapacity(DWORD &bytes, DWORD &sectors)
{
    bytes   = BCapacity;
    sectors = SCapacity;
}

bool TranslatedBootMedia::readSectors(DWORD sectorNo, DWORD count, BYTE *bfr)
{
    if(!isInit()) {                             // if not initialized, failed
        return false;
    }

	if(sectorNo >= SCapacity) {					// if trying to read sector beyond the last sector, fail
		return false;
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

bool TranslatedBootMedia::writeSectors(DWORD sectorNo, DWORD count, BYTE *bfr)
{
	return false;								// write not supported
}
