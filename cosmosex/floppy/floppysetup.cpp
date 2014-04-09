#include <stdio.h>

#include "floppysetup.h"

FloppySetup::FloppySetup()
{
    dataTrans = NULL;
}

FloppySetup::~FloppySetup()
{

}

void FloppySetup::processCommand(BYTE *cmd)
{

}

void FloppySetup::setAcsiDataTrans(AcsiDataTrans *dt)
{
    dataTrans = dt;
}

