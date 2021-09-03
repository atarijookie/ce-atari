// vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab
#ifndef _RESOLVE_H_
#define _RESOLVE_H_

#include <netdb.h>
#include <string>

#include <stdint.h>

#define RESOLV_COUNT    10
struct Tresolv {
             uint32_t          startTime;
    volatile uint8_t           getaddrinfoHasFinished;
    volatile uint8_t           processed;
             int            error;

             char           hostName[256];
             char           canonName[256];

             int            count;
             uint8_t           data[128];
             std::string    h_name;

             gaicb          req;
             addrinfo       hints;
};

class ResolverRequest {
public:
    int  addRequest         (const char *hostName);
    bool checkAndhandleSlot (int index);
    void showSlot           (int index);
    void clearSlot          (int index);
    bool slotIndexValid     (int index);

    Tresolv requests[RESOLV_COUNT];

private:
    bool getaddrinfoHasFinished (int index);
    bool isProcessed            (int index);
    bool processSlot            (int index);
};

#endif
