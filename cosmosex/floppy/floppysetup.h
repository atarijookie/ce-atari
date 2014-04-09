#ifndef _FLOPPYSETUP_H_
#define _FLOPPYSETUP_H_

#include <stdio.h>

#include "../settingsreloadproxy.h"
#include "../datatypes.h"

class AcsiDataTrans;

class FloppySetup
{
public:
    FloppySetup();
    ~FloppySetup();

    void processCommand(BYTE *cmd);
    void setAcsiDataTrans(AcsiDataTrans *dt);

private:
    AcsiDataTrans       *dataTrans;


};

#endif

