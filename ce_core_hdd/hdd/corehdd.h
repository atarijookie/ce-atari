#ifndef COREHDD_H
#define COREHDD_H

#include "../misc/global.h"
#include "../misc/settings.h"
#include "../misc/version.h"
#include "../misc/utils.h"

#define MAX_CLIENTS     8
class HddClient;

class CoreHdd
{
public:
    CoreHdd();
    ~CoreHdd();

    void run(void);

private:
    HddClient* hddClients[MAX_CLIENTS];

    void handleEvents(void);
    void displayStatusToConsole(uint32_t now);
};

class LoadTracker {
public:
    struct {
        uint32_t start;
        uint32_t total;
    } cycle;

    struct {
        void markStart(void) {                          // call on start of block where the work is done (exclude idle sleep())
            start  = Utils::getCurrentMs();
        }

        void markEnd(void) {                            // call on end of block where the work is done (exclude idle sleep())
            total += Utils::getCurrentMs() - start;
        }

        uint32_t start;
        uint32_t total;
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
        loadPercents = 0;
        suspicious  = false;

        cycle.total = 0;
        cycle.start = Utils::getCurrentMs();
        busy.total  = 0;
    }
};

#endif // COREHDD_H
