#ifndef _IMAGESILO_H_
#define _IMAGESILO_H_

#include <string>
#include "../datatypes.h"

typedef struct 
{
    std::string imageFile;      // just file name:                     bla.st
    std::string hostDestPath;   // where the file is stored when used: /tmp/bla.st
    std::string atariSrcPath;   // from where the file was uploaded:   C:\gamez\bla.st
    std::string hostSrcPath;    // for translated disk, host path:     /mnt/sda/gamez/bla.st

    std::string content;
} SiloSlot;

class ImageSilo
{
public:
    ImageSilo();    

    void add(int positionIndex, std::string &filename, std::string &hostDestPath, std::string &atariSrcPath, std::string &hostSrcPath);
    void swap(int index);
    void remove(int index);

    void dumpStringsToBuffer(BYTE *bfr);

private:
    SiloSlot slots[3];
};

#endif

