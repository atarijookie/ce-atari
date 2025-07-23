#ifndef FLOPPYTHREAD_H
#define FLOPPYTHREAD_H

#include "global.h"
#include "settings.h"

#include "utils.h"

#define INBUF_SIZE  16384

class FloppyThread
{
public:
    FloppyThread();
    virtual ~FloppyThread();

    void run(void);
    virtual void reloadSettings(int type);                                  // from ISettingsUser
    bool handleOneClient(int clientIndex, int fdClient, int floppySlotIndex);

private:
    bool shouldRun;
    bool running;

    //-----------------------------------
    // settings and config stuff

    void loadSettings(void);

    //-----------------------------------
    // floppy stuff

    bool handleFdd(int clientIndex, int fdClient, int floppySlotIndex, uint8_t* inBuff);
    void handleFwVersion_franz(int clientIndex);
    void handleSendTrack(int clientIndex);
    void handleSendImage(int clientIndex);
    void handleSectorWritten(int clientIndex);

    void loadLastImageIntoSlot(int clientIndex);

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
