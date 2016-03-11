#ifndef _RESOLVE_H_
#define _RESOLVE_H_

#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h> 
#include <fcntl.h>
#include <sys/ioctl.h>

#include <poll.h>
#include <signal.h>
#include <pthread.h>

#include <stdint.h>
#include <string>

#include "../datatypes.h"

#define RESOLV_COUNT    10
struct Tresolv {
             DWORD          startTime;
    volatile BYTE           getaddrinfoHasFinished;
    volatile BYTE           processed;
             int            error;

             char           hostName[256];
             char           canonName[256];

             int            count;
             BYTE           data[128];
             std::string    h_name;

             gaicb          req;
             addrinfo       hints;
};

class ResolverRequest {
public:
    int  addRequest         (char *hostName);       
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

