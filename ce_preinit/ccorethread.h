#ifndef CCORETHREAD_H
#define CCORETHREAD_H

#include "utils.h"
#include "global.h"
#include "conspi.h"
#include "acsidatatrans.h"
#include "settings.h"

#include "version.h"

class CCoreThread
{
public:
    CCoreThread(void);
    ~CCoreThread();

	void resetHansAndFranz(void);
    void run(void);

    void sendHalfWord(void);
    void reloadSettings(int type);    								// from ISettingsUser

    void setFloppyImageLed(int ledNo);

private:
    bool shouldRun;
    bool running;

    //-----------------------------------
    // data connection to STM32 slaves over SPI
    CConSpi         *conSpi;
    AcsiDataTrans   *dataTrans;

    //-----------------------------------
    // settings and config stuff

    void loadSettings(void);

    //-----------------------------------
    // hard disk stuff
    bool            setEnabledIDbits;
	AcsiIDinfo		acsiIdInfo;
    
    void handleAcsiCommand(void);

    //-----------------------------------
    // handle FW version
    void handleFwVersion_hans(void);
    void handleFwVersion_franz(void);
    int bcdToInt(int bcd);

    void responseStart(int bufferLengthInBytes);
    void responseAddWord(BYTE *bfr, WORD value);
    void responseAddByte(BYTE *bfr, BYTE value);

    struct {
        int bfrLengthInBytes;
        int currentLength;
    } response;

    void convertXilinxInfo(BYTE xilinxInfo);
    void saveHwConfig(void);
    void getIdBits(BYTE &enabledIDbits, BYTE &sdCardAcsiId);
    
    struct {
        struct {
            WORD acsi;
            WORD fdd;
        } current;
        
        struct {
            WORD acsi;
            WORD fdd;
        } next;
        
        bool skipNextSet;
    } hansConfigWords;

    //-----------------------------------
    // other
};

class LoadTracker {
public: 
    struct {
        DWORD start;
        DWORD total;
    } cycle;

    struct {
        void markStart(void) {                          // call on start of block where the work is done (exclude idle sleep())
            start  = Utils::getCurrentMs();
        }

        void markEnd(void) {                            // call on end of block where the work is done (exclude idle sleep())
            total += Utils::getCurrentMs() - start;
        }

        DWORD start;
        DWORD total;
    } busy;
    
    int     loadPercents;                               // contains 0 .. 100, meaning percentage of load
    bool    suspicious;                                 // if the last load percentage was high or cycle time was long, this will be true
    
    LoadTracker(void) {
        clear();
    }

    void calculate(void) {  // call this on the end of 1 second interval to calculate load 
        cycle.total     = Utils::getCurrentMs() - cycle.start;
        loadPercents    = (busy.total * 100) / cycle.total;
        
        suspicious      = false;
        
        if(cycle.total > 1100 || loadPercents > 90) {
            suspicious  = true;
        }
    }
    
    void clear(void) {      // call this on the start of new 1 second interval to clear everything
        loadPercents= 0;
        suspicious  = false;

        cycle.total = 0;
        cycle.start = Utils::getCurrentMs();
        busy.total  = 0;
    }
};

#endif // CCORETHREAD_H
