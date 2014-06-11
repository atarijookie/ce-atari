#include <stdio.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <unistd.h>
#include <fcntl.h>

#include "devicemedia.h"
#include "../debug.h"

DeviceMedia::DeviceMedia()
{
    BCapacity       = 0;
    SCapacity       = 0;
    mediaHasChanged = 0;
}

DeviceMedia::~DeviceMedia()
{
    iclose();
}

bool DeviceMedia::iopen(char *path, bool createIfNotExists)
{
    mediaHasChanged = false;

	fdes = open(path, O_RDWR);					// open device for Read / Write

	if (fdes < 0) {								// failed to open for R/W?
		fdes = open(path, O_RDONLY);			// try to open the device for reading only

		if(fdes < 0) {							// failed to open as read only? damn...
			return false;
		}
	}


	DWORD size;
	int res = ioctl(fdes, BLKGETSIZE, &size);	// try to get device capacity in sectors
	
	if(res < 0) {								// failed to get capacity?
		return false;
	}

    BCapacity = size * 512;                		// capacity in bytes
    SCapacity = size;              				// capacity in sectors

    mediaHasChanged = false;

    Debug::out(LOG_INFO, "DeviceMedia - open succeeded, capacity: %d MB", (int) (BCapacity / (1024 * 1024)));

    return true;
}

void DeviceMedia::iclose(void)
{
    if(fdes > 0) {
       	close(fdes);															// close device
    }

    BCapacity       = 0;
    SCapacity       = 0;
    mediaHasChanged = 0;
}

bool DeviceMedia::isInit(void)
{
    if(fdes > 0) {
        return true;
    }

    return false;
}

bool DeviceMedia::mediaChanged(void)
{
    return mediaHasChanged;
}

void DeviceMedia::setMediaChanged(bool changed)
{
    mediaHasChanged = changed;
}

void DeviceMedia::getCapacity(DWORD &bytes, DWORD &sectors)
{
    bytes   = BCapacity;
    sectors = SCapacity;
}

bool DeviceMedia::readSectors(DWORD sectorNo, DWORD count, BYTE *bfr)
{
    if(!isInit()) {                             // if not initialized, failed
        return false;
    }

    off_t pos = sectorNo * 512;					// convert sector # to offset 
	int res = lseek(fdes, pos, SEEK_SET);

    if(res < 0) {                              	// failed to lseek?
        return false;
    }

    size_t byteCount = count * 512;
	size_t cnt = read(fdes, bfr, byteCount);	// try to read sector(s)
	
    if(cnt != byteCount) {                      // not all data was read? fail
        return false;
    }

    return true;
}

bool DeviceMedia::writeSectors(DWORD sectorNo, DWORD count, BYTE *bfr)
{
    if(!isInit()) {                             // if not initialized, failed
        return false;
    }

    off_t pos = sectorNo * 512;					// convert sector # to offset 
	int res = lseek(fdes, pos, SEEK_SET);

    if(res < 0) {                              	// failed to lseek?
        return false;
    }

    size_t byteCount = count * 512;
	size_t cnt = write(fdes, bfr, byteCount);	// try to write sector(s)
	
    if(cnt != byteCount) {                      // not all data was writen? fail
        return false;
    }
	
    return true;
}
