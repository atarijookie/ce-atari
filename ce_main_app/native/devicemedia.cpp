#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>

#include <stdio.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <fcntl.h>
#include <errno.h>

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

bool DeviceMedia::iopen(const char *path, bool createIfNotExists)
{
    mediaHasChanged = false;

	fdes = open(path, O_RDWR | O_LARGEFILE);	    // open device for Read / Write

	if (fdes < 0) {								    // failed to open for R/W?
		fdes = open(path, O_RDONLY | O_LARGEFILE);  // try to open the device for reading only

		if(fdes < 0) {							    // failed to open as read only? damn...
			return false;
		}
	}


	DWORD size;
	int res = ioctl(fdes, BLKGETSIZE, &size);	// try to get device capacity in sectors
	
	if(res < 0) {								// failed to get capacity?
		return false;
	}

    SCapacity = size;              				    // capacity in sectors
    BCapacity = ((int64_t) size) * ((int64_t)512);  // capacity in bytes

    mediaHasChanged = false;

    int64_t capacityInMB = BCapacity / ((int64_t) (1024 * 1024));
    
    Debug::out(LOG_DEBUG, "DeviceMedia - open succeeded, capacity: %d MB, sectors: %08x", (int) capacityInMB, SCapacity);

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

void DeviceMedia::getCapacity(int64_t &bytes, int64_t &sectors)
{
    bytes   = BCapacity;
    sectors = SCapacity;
}

bool DeviceMedia::readSectors(int64_t sectorNo, DWORD count, BYTE *bfr)
{
    if(!isInit()) {                             // if not initialized, failed
        Debug::out(LOG_DEBUG, "DeviceMedia::readSectors - not init");
        return false;
    }

#if defined(_FILE_OFFSET_BITS) && (_FILE_OFFSET_BITS == 64)
	off_t pos = sectorNo * 512;
	off_t ofs = lseek(fdes, pos, SEEK_SET);
	if(ofs != pos) {
		Debug::out(LOG_ERROR, "DeviceMedia::readSectors - lseek failed %lld", (long long)ofs, strerror(errno));
		return false;
	}
#else
	/* Note : why not using lseek64() ? (#define _LARGEFILE64_SOURCE) */
    off64_t pos = sectorNo * ((int64_t)512);    // convert sector # to offset 
    loff_t loff;
    int res = syscall(__NR__llseek, fdes, (unsigned long) (pos >> 32), (unsigned long) pos, &loff, SEEK_SET);

    if(res < 0) {                              	// failed to lseek?
        Debug::out(LOG_DEBUG, "DeviceMedia::readSectors - lseek64() failed, errno: %d", errno);
        return false;
    }
#endif

    size_t byteCount = count * 512;
	size_t cnt = read(fdes, bfr, byteCount);	// try to read sector(s)
	
    if(cnt != byteCount) {                      // not all data was read? fail
        Debug::out(LOG_DEBUG, "DeviceMedia::readSectors - read() failed");
        return false;
    }

    return true;
}

bool DeviceMedia::writeSectors(int64_t sectorNo, DWORD count, BYTE *bfr)
{
    if(!isInit()) {                             // if not initialized, failed
        Debug::out(LOG_DEBUG, "DeviceMedia::writeSectors - not init");
        return false;
    }

#if defined(_FILE_OFFSET_BITS) && (_FILE_OFFSET_BITS == 64)
	off_t pos = sectorNo * 512;
	off_t ofs = lseek(fdes, pos, SEEK_SET);
	if(ofs != pos) {
		Debug::out(LOG_ERROR, "DeviceMedia::writeSectors - lseek failed %lld", (long long)ofs, strerror(errno));
		return false;
	}
#else
    off64_t pos = sectorNo * ((int64_t)512);    // convert sector # to offset 
    loff_t loff;
    int res = syscall(__NR__llseek, fdes, (unsigned long) (pos >> 32), (unsigned long) pos, &loff, SEEK_SET);

    if(res < 0) {                              	// failed to lseek?
        Debug::out(LOG_DEBUG, "DeviceMedia::writeSectors - lseek64() failed, errno: %d", errno);
        return false;
    }
#endif

    size_t byteCount = count * 512;
	size_t cnt = write(fdes, bfr, byteCount);	// try to write sector(s)
	
    if(cnt != byteCount) {                      // not all data was writen? fail
        Debug::out(LOG_DEBUG, "DeviceMedia::writeSectors - write() failed");
        return false;
    }
	
    return true;
}
