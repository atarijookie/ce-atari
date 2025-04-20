// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
#ifndef FLOPPYIMAGEMSA_H
#define FLOPPYIMAGEMSA_H

#include "floppyimage.h"

class FloppyImageMsa: public FloppyImage
{
public:
    virtual bool open(const char *fileName);
    virtual bool save(void);

protected:
    virtual bool loadImageIntoMemory(void);
};

#endif // FLOPPYIMAGEST_H
