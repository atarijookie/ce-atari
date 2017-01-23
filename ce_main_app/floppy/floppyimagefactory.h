// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
#ifndef FLOPPYIMAGEFACTORY_H
#define FLOPPYIMAGEFACTORY_H

#include "floppyimage.h"

class FloppyImageFactory
{
public:
    static FloppyImage *getImage(const char *fileName);

private:
    static void toLowerCase(char *orig, char *lower);
    static char lowerCase(char in);

    static bool handleZIPedImage(const char *inZipFilePath, char *outImageFilePath);
};

#endif // FLOPPYIMAGEFACTORY_H
