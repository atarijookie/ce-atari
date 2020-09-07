#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include "../utils.h"
#include "../debug.h"
#include "../global.h"
#include "../update.h"
#include "spisd.h"

extern THwConfig hwConfig;

//------------------------------------
// ChipInterface v3 contains also object for accessing SD card as that is done via RPi SPI on v3.
// This is a global object, because it will be accessed by CoreThread when Atari reads or writes to SD card,
// but also it will be accessed by other thread, which will detect card insertion and removal.
// This object is created and destroyed elsewhere, so accesss it only when the pointer is not null.
extern SpiSD *spiSd;
//------------------------------------

void *sdThreadCode(void *ptr)
{
    DWORD nextCardCheck = Utils::getEndTime(1000);      // check in a while
    Debug::out(LOG_DEBUG, "sdThreadCode starting");

    while(sigintReceived == 0) {                        // while not terminated
        if(spiSd == NULL) {                             // if the SD via SPI object doesn't exist, nothing to do here
            //Debug::out(LOG_DEBUG, "sdThreadCode - spiSd object is NULL");
            Utils::sleepMs(1000);
            continue;
        }

        // TODO: do rest only when other threads not accessing spiSd object

        if(Utils::getCurrentMs() >= nextCardCheck) {    // should check for card now?
            nextCardCheck = Utils::getEndTime(1000);    // check in a while
            BYTE res;

            if(spiSd->isInitialized()) {                 // when init, check if not removed
                //Debug::out(LOG_DEBUG, "sdThreadCode - checking if SD card still available");
                res = spiSd->mmcReadJustForTest(0);

                if(res != 0) {                          // when read failed, card probably removed
                    Debug::out(LOG_DEBUG, "sdThreadCode - SD card removed");
                    spiSd->clearStruct();
                }
            } else {                                    // when not init, try to init now
                //Debug::out(LOG_DEBUG, "sdThreadCode - trying to init SD card");
                spiSd->initCard();                       // try to init the card

                if(spiSd->isInitialized()) {             // if card initialized
                    Debug::out(LOG_DEBUG, "sdThreadCode - SD card found and initialized");
                }
            }
        }

        Utils::sleepMs(500);                            // sleep a while
    }

    Debug::out(LOG_DEBUG, "sdThreadCode finished");
    return 0;
}
