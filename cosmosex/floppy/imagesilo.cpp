#include <stdio.h>
#include <string.h>

#include "imagesilo.h"

ImageSilo::ImageSilo()
{
    for(int i=0; i<3; i++) {
        slots[i].imageFile.clear();
        slots[i].content.clear();
    }
}

void ImageSilo::add(int positionIndex, std::string &filename, std::string &hostDestPath, std::string &atariSrcPath, std::string &hostSrcPath)
{
    if(positionIndex < 0 || positionIndex > 2) {
        return;
    }

    slots[positionIndex].imageFile      = filename;         // just file name:                     bla.st
    slots[positionIndex].hostDestPath   = hostDestPath;     // where the file is stored when used: /tmp/bla.st
    slots[positionIndex].atariSrcPath   = atariSrcPath;     // from where the file was uploaded:   C:\gamez\bla.st
    slots[positionIndex].hostSrcPath    = hostSrcPath;      // for translated disk, host path:     /mnt/sda/gamez/bla.st

    // TODO: fill slots[positionIndex].content with the content of image (according to the list of images)
}

void ImageSilo::swap(int index)
{
    if(index < 0 || index > 2) {
        return;
    }

    std::string tmp;

    SiloSlot *a, *b;
    
    // find out which two slots to swap
    switch(index) {
        case 0:
            a = &slots[0];
            b = &slots[1];
            break;

        case 1:
            a = &slots[1];
            b = &slots[2];
            break;

        case 2:
            a = &slots[2];
            b = &slots[0];
            break;
    }

    // swap image files
    tmp = a->imageFile;
    a->imageFile = b->imageFile;
    b->imageFile = tmp;

    // swap image content description
    tmp = a->content;
    a->content = b->content;
    b->content = tmp;
}

void ImageSilo::remove(int index)                   // remove image at specified slot
{
    if(index < 0 || index > 2) {
        return;
    }

    slots[index].imageFile.clear();
    slots[index].content.clear();
}

void ImageSilo::dumpStringsToBuffer(BYTE *bfr)      // copy the strings to buffer
{
    memset(bfr, 0, 512);

    for(int i=0; i<3; i++) {
        strncpy((char *) &bfr[(i * 160)     ], (char *) slots[i].imageFile.c_str(), 79);
        strncpy((char *) &bfr[(i * 160) + 80], (char *) slots[i].content.c_str(),   79);
    }
}



