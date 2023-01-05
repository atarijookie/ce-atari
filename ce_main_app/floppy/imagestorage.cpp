#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <string>
#include <iostream>
#include <sstream>
#include <algorithm>

#include <unistd.h>

#include "imagestorage.h"
#include "../utils.h"
#include "../debug.h"
#include "../translated/translateddisk.h"

ImageStorage::ImageStorage(void)
{

}

bool ImageStorage::doWeHaveStorage(void)                         // returns true if shared drive or usb drive is attached, returns false if not (can't download images then)
{
    std::string hostRootPath;               // path to where we will store data
    bool res = getStoragePath(hostRootPath);    // get path of first USB drive or to shared drive

    Debug::out(LOG_DEBUG, "ImageStorage::doWeHaveStorate() -> %d", res);

    return res;
}

bool ImageStorage::getStoragePath(std::string &storagePath)             // returns where the images are now stored
{
    storagePath = Utils::dotEnvValue("DOWNLOAD_STORAGE_DIR");
    Debug::out(LOG_DEBUG, "ImageStorage::getStoragePath() -> %s", storagePath.c_str());
    return true;
}

bool ImageStorage::getImageLocalPath(const char *imageFileName, std::string &path)  // fills the path with full path to the specified image
{
    std::string filename = imageFileName;   // to std::string

    bool res = getStoragePath(path);        // get path to storage
    if(!res) {                              // no path? fail
        Debug::out(LOG_DEBUG, "ImageStorage::getImageLocalPath() - fail, no storage path");
        return res;
    }

    Utils::mergeHostPaths(path, filename);  // merge path and filename (path+filename is now in path)

	// get extension, and if the extension of image is ZIP, check if we have the same file but with .msa or .st extension (it might be already extracted)
    bool isZIPfile = Utils::isZIPfile(imageFileName);

    if(isZIPfile) {                                     // if it's ZIP file, additional handling happens
        std::string pathMsa, pathSt;
        Utils::createPathWithOtherExtension(path, "msa", pathMsa);  // create filename with .msa extension

        if(Utils::fileExists(pathMsa)) {    // the .msa file exists? store it and use it
            path = pathMsa;
            Debug::out(LOG_DEBUG, "ImageStorage::getImageLocalPath() - found MSA: %s -> %s", imageFileName, path.c_str());
            return true;
        }

        Utils::createPathWithOtherExtension(path, "st", pathSt);    // create filename with .st extension

        if(Utils::fileExists(pathSt)) {     // the .st file exists? store it and use it
            path = pathSt;
            Debug::out(LOG_DEBUG, "ImageStorage::getImageLocalPath() - found ST: %s -> %s", imageFileName, path.c_str());
            return true;
        }
    }

    // if got here, it's either not ZIP file, or the expected extracted image files don't exist -- just return the generated path
    Debug::out(LOG_DEBUG, "ImageStorage::getImageLocalPath() - %s -> %s", imageFileName, path.c_str());
    return true;
}

bool ImageStorage::weHaveThisImage(const char *imageFileName)    // returns if floppy image with this filename is stored in our storage
{
    std::string path;
    getImageLocalPath(imageFileName, path); // create full local path out of filename

    bool exists = Utils::fileExists(path.c_str());   // try to access the file
    return exists;
}
