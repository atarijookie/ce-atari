#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <iostream>
#include <sstream>
#include <algorithm>

#include <unistd.h>

#include "imagestorage.h"
#include "../utils.h"
#include "../downloader.h"
#include "../debug.h"
#include "../translated/translateddisk.h"

ImageStorage::ImageStorage(void)
{

}

bool ImageStorage::doWeHaveStorage(void)                         // returns true if shared drive or usb drive is attached, returns false if not (can't download images then)
{
    std::string hostRootPath;               // path to where we will store data
    return getStoragePath(hostRootPath);    // get path of first USB drive or to shared drive
}

bool ImageStorage::getStoragePath(std::string &storagePath)             // returns where the images are now stored
{
    storagePath.clear();

    TranslatedDisk * translated = TranslatedDisk::getInstance();

    if(!translated) {           // no translated disk instance? no storage
        return false;
    }

    translated->mutexLock();
    bool gotStorage = translated->getPathToUsbDriveOrSharedDrive(storagePath);  // get path of first USB drive or to shared drive
    translated->mutexUnlock();

    if(!gotStorage) {           // don't have storage? quit
        return false;
    }

    std::string subdir = IMAGE_STORAGE_SUBDIR;
    Utils::mergeHostPaths(storagePath, subdir);     // to the storage root path add the subdir

    return true;
}

bool ImageStorage::getImageLocalPath(const char *imageFileName, std::string &path)  // fills the path with full path to the specified image
{
    std::string filename = imageFileName;   // to std::string

    bool res = getStoragePath(path);        // get path to storage
    if(!res) {                              // no path? fail
        return res;
    }

    Utils::mergeHostPaths(path, filename);  // merge path and filename
    return res;
}

void ImageStorage::scanStorageForImages(void)                    // go through the storage and find all the floppy images there
{
    std::string path;

    bool res = getStoragePath(path);        // get path to storage
    if(!res) {                              // no path? fail
        return;
    }



    // TODO: implement scan
}

bool ImageStorage::weHaveThisImage(const char *imageFileName)    // returns if floppy image with this filename is stored in our storage
{
    // TODO: remake by checking agains the list of files retrieved by scanStorageForImages()

    std::string path;
    getImageLocalPath(imageFileName, path); // create full local path out of filename

    int res = access(path.c_str(), F_OK);   // try to access the file

    if(res != -1) {                         // if it's not this error, then the file exists
        return true;
    }

    return false;                           // if got here, it's a fail
}
