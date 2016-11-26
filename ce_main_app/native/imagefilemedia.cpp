// vim: tabstop=4 shiftwidth=4 expandtab
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
//#include <stdio.h>
#include <string.h>

#include "imagefilemedia.h"
#include "../debug.h"

ImageFileMedia::ImageFileMedia()
: BCapacity(0), SCapacity(0), mediaHasChanged(0), fd(-1)
{
}

ImageFileMedia::~ImageFileMedia()
{
    iclose();
}

bool ImageFileMedia::iopen(const char *path, bool createIfNotExists)
{
    bool imageWasCreated = false;
    mediaHasChanged = false;

    // try to open existing file
    fd = open(path, O_RDWR);	// O_RDWR / O_RDONLY ?  O_LARGEFILE

    if(fd == -1 && createIfNotExists) {     // failed to open existing file and should create one?
        fd = open(path, O_RDWR|O_CREAT, 0666);
        imageWasCreated = true;
    }

    if(fd == -1) {
        Debug::out(LOG_ERROR, "ImageFileMedia - failed to open %s : %s", path, strerror(errno));
        return false;
    }

    if(imageWasCreated) {               // if the image was just created, create empty image file
        BYTE bfr[512];
        memset(bfr, 0, 512);

        Debug::out(LOG_DEBUG, "ImageFileMedia - creating %s, filling with 1MB of 0",path);
        for(int i=0; i<2048; i++) {     // create 1 MB image
            write(fd, bfr, 512);
        }

        fsync(fd);
    }

    struct stat st;
    if(fstat(fd, &st) < 0) {
        Debug::out(LOG_ERROR, "fstat(%d) %s FAILED : %s", fd, path, strerror(errno));
    } else {
        BCapacity = st.st_size;                    // capacity in bytes
        SCapacity = st.st_size / 512;              // capacity in sectors
    }

    mediaHasChanged = false;

    Debug::out(LOG_DEBUG, "ImageFileMedia - open succeeded, capacity: %lld sectors / %lld bytes, was created: %d",
               (long long)SCapacity, (long long)BCapacity, (int)imageWasCreated);

    return true;
}

void ImageFileMedia::iclose(void)
{
    if(fd >= 0) {
        close(fd);
    }

    BCapacity       = 0;
    SCapacity       = 0;
    mediaHasChanged = 0;
    fd              = -1;
}

bool ImageFileMedia::isInit(void)
{
    return (fd >= 0);
}

bool ImageFileMedia::mediaChanged(void)
{
    return mediaHasChanged;
}

void ImageFileMedia::setMediaChanged(bool changed)
{
    mediaHasChanged = changed;
}

void ImageFileMedia::getCapacity(int64_t &bytes, int64_t &sectors)
{
    bytes   = BCapacity;
    sectors = SCapacity;
}

bool ImageFileMedia::readSectors(int64_t sectorNo, DWORD count, BYTE *bfr)
{
    if(!isInit()) {                             // if not initialized, failed
        return false;
    }

    off_t pos = sectorNo * 512;                 // convert sector # to offset in image file
    off_t ofs = lseek(fd, pos, SEEK_SET);       // move to the end of file

    if(ofs != pos) {                              // failed to seek?
        Debug::out(LOG_ERROR, "ImageFileMedia - lseek failed %lld %s", (long long)ofs, strerror(errno));
        return false;
    }

    size_t byteCount = count * 512;
    while(byteCount > 0) {
        ssize_t res = read(fd, bfr, byteCount);
        if(res < 0) {
            Debug::out(LOG_ERROR, "ImageFileMedia - read error : %s", strerror(errno));
            return false;
        } else if(res == 0) {
            Debug::out(LOG_ERROR, "ImageFileMedia - read returned 0 (byteCount=%llu)", (long long unsigned)byteCount);
            return false;
        }
        byteCount -= res;
        bfr += res;
    }

    return true;
}

bool ImageFileMedia::writeSectors(int64_t sectorNo, DWORD count, BYTE *bfr)
{
    if(!isInit()) {                             // if not initialized, failed
        return false;
    }

    off_t pos = sectorNo * 512;                 // convert sector # to offset in image file
    off_t ofs = lseek(fd, pos, SEEK_SET);       // move to the end of file

    if(ofs != pos) {                              // failed to seek?
        Debug::out(LOG_ERROR, "ImageFileMedia - lseek failed %lld %s", ofs, strerror(errno));
        return false;
    }

    size_t byteCount = count * 512;
    while(byteCount > 0) {
        ssize_t res = write(fd, bfr, byteCount);
        if(res < 0) {
            Debug::out(LOG_ERROR, "ImageFileMedia - write error : %s", strerror(errno));
            return false;
        } else if(res == 0) {
            Debug::out(LOG_ERROR, "ImageFileMedia - write returned 0");
            return false;
        }
        byteCount -= res;
        bfr += res;
    }

    return true;
}
