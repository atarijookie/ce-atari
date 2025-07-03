#ifndef FLOPPYTHREAD_H
#define FLOPPYTHREAD_H

#include "global.h"
#include "settings.h"

#include "settingsreloadproxy.h"
#include "isettingsuser.h"

#include "version.h"
#include "utils.h"

#define INBUF_SIZE  16384

class FloppyThread: public ISettingsUser
{
public:
    FloppyThread();
    virtual ~FloppyThread();

    void run(void);
    virtual void reloadSettings(int type);                                  // from ISettingsUser
    bool handleOneClient(int fdClient, int floppySlotindex);

private:
    bool shouldRun;
    bool running;

    //-----------------------------------
    // settings and config stuff

    void loadSettings(void);

    //-----------------------------------
    // floppy stuff

    bool handleFdd(int fdClient, int floppySlotindex, uint8_t* inBuff);
    void handleFwVersion_franz(int fdClient);
    void handleSendTrack(int fdClient, int floppySlotindex, uint8_t *inBuf);
    void handleSectorWritten(int fdClient, int floppySlotindex);

    //----------------------------------
    // recovery stuff
    void insertSpecialFloppyImage(int specialImageId);

    //----------------------------------
    // other
    void sharedObjects_create(void);
    void sharedObjects_destroy(void);

    void displayStatusToConsole(uint32_t now);
};

#endif // FLOPPYTHREAD_H
