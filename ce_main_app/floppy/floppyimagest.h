// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
#ifndef FLOPPYIMAGEST_H
#define FLOPPYIMAGEST_H

#include <stdio.h>
#include "floppyimage.h"

#include "../datatypes.h"

class FloppyImageSt: public FloppyImage
{
public:
    virtual bool open(const char *fileName);
    virtual bool saveImage();
private:
    bool calcParams(void);
};

#endif // FLOPPYIMAGEST_H
