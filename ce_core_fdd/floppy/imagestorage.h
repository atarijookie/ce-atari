#ifndef _IMAGESTORAGE_H_
#define _IMAGESTORAGE_H_

#include <map>
#include <string>
#include <vector>
#include <sstream>

#define IMAGE_STORAGE_SUBDIR  "floppy_images"

class ImageStorage
{
public:
    ImageStorage(void);

    bool doWeHaveStorage(void);                         // returns true if shared drive or usb drive is attached, returns false if not (can't download images then)
    bool getStoragePath(std::string &storagePath);      // returns where the images are now stored
    bool getImageLocalPath(const char *imageFileName, std::string &path);  // fills the path with full path to the specified image
    bool weHaveThisImage(const char *imageFileName);    // returns if floppy image with this filename is stored in our storage

private:

};

#endif

