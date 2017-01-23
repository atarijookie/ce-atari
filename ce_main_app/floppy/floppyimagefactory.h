// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
#ifndef FLOPPYIMAGEFACTORY_H
#define FLOPPYIMAGEFACTORY_H

#include "ifloppyimage.h"
#include "floppyimagest.h"
#include "floppyimagemsa.h"

class FloppyImageFactory
{
public:
    FloppyImageFactory();
    ~FloppyImageFactory();

    IFloppyImage *getImage(const char *fileName);

private:
    FloppyImageMsa  *msa;
    FloppyImageSt   *st;

    void toLowerCase(char *orig, char *lower);
    char lowerCase(char in);

    bool handleZIPedImage(const char *inZipFilePath, char *outImageFilePath);
};

#endif // FLOPPYIMAGEFACTORY_H
