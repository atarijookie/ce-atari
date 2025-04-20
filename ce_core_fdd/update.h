#ifndef _UPDATE_H
#define _UPDATE_H

#include "global.h"
#include "version.h"

class Update
{
public:
    static Versions versions;
    static void initialize(void);
};

#endif
