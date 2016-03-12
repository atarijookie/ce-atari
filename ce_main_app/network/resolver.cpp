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

#include "../debug.h"
#include "../utils.h"
#include "resolver.h"

#define IS_VALID_IP_NUMBER(X)  (X >= 0 && X <= 255)

//---------------------------------
int ResolverRequest::addRequest(char *hostName)
{
    int i, emptyIndex = -1;

    DWORD   oldestTime  = 0x7fffffff;
    int     oldestIndex = -1;

    // find usable slot
    for(i=0; i<RESOLV_COUNT; i++) {
        if(requests[i].startTime == 0) {      // start time 0 means it's empty (not running)
            emptyIndex = i;
            break;
        }
                                            // if it's running and it's older than the currently found oldest, mark it
        if(oldestTime > requests[i].startTime) {
            oldestTime  = requests[i].startTime;
            oldestIndex = i;
        }
    }

    // get pointer to that slot
    Tresolv *r;
    int      usedIndex;

    if(emptyIndex != -1) {      // if found empty slot, use it
        r = &requests[emptyIndex];
        usedIndex = emptyIndex;
    } else {                    // if not found empty slot, cancel existing and use it
        gai_cancel(&requests[oldestIndex].req);       // try to cancel
        r = &requests[oldestIndex];
        usedIndex = oldestIndex;
    }

    r->startTime                = Utils::getCurrentMs();    // store start time
    r->getaddrinfoHasFinished   = 0;                        // mark that we did not finish yet
    r->count                    = 0;                        // no IPs stored yet
    r->processed                = 0;                        // not processed yet
    memset(r->canonName, 0, 256);                           // no canon name yet

    memset (r->hostName, 0, 256);
    strncpy(r->hostName, hostName, 255);                    // copy host name to our array
    r->req.ar_name      = r->hostName;                      // store pointer to that array

    //-------------------
    // check and possibly resolve dotted IP
    int a,b,c,d;
    int iRes = sscanf(hostName, "%d.%d.%d.%d", &a, &b, &c, &d);
    
    if(iRes == 4) {         // succeeded to get IP parts
        if(IS_VALID_IP_NUMBER(a) && IS_VALID_IP_NUMBER(b) && IS_VALID_IP_NUMBER(c) && IS_VALID_IP_NUMBER(d)) {

            BYTE *pIps = (BYTE *) r->data;
            pIps[0]     = a;    // store that IP
            pIps[1]     = b;
            pIps[2]     = c;
            pIps[3]     = d;
            
            r->count    = 1;    // store count
            
            strcpy(r->canonName, r->hostName);                          // pretend that the string is canonical name
            
            r->getaddrinfoHasFinished   = 1;                            // resolve finished
            r->processed                = 1;
            
            return usedIndex;
        }
    }
    //-------------------
    
    r->hints.ai_flags   = AI_CANONNAME;                     // get the official name of the host
    r->req.ar_request   = &r->hints;

    gaicb *pReq = &r->req;
    int ret = getaddrinfo_a(GAI_NOWAIT, &pReq, 1, NULL);

    if (ret) {
        Debug::out(LOG_DEBUG, "addRequest() failed");
        return -1;
    }

    Debug::out(LOG_DEBUG, "addRequest() - resolving %s under index %d", hostName, usedIndex);
    return usedIndex;           // return index under which this request runs
}

//---------------------------------
bool ResolverRequest::getaddrinfoHasFinished(int index)    // returns true if request has finished
{
    if(!slotIndexValid(index)) {                // out of bounds? false
        return false;
    }

    if(requests[index].startTime == 0) {        // not used? false
        return false;
    }

    Tresolv *r = &requests[index];

    if(r->getaddrinfoHasFinished) {             // if the request already finished, good
        return true;
    }

    // if we don't know if it finished yet, let's check it out
    r->error = gai_error(&r->req);

    if(r->error == EAI_INPROGRESS) {            // if still in progress, false
        return false;
    }

    r->getaddrinfoHasFinished = 1;              // ok, we're done
    return true;
}

//---------------------------------

bool ResolverRequest::isProcessed(int index)                // see if the reqest is processed
{
    if(!slotIndexValid(index)) {                            // out of bounds? pretend it's processed
        return true;
    }

    Tresolv *r = &requests[index];

    if(!r->getaddrinfoHasFinished) {                        // if not done, then it's not processed
        return false;
    }

    return (r->processed == 1);                             // return if it's processed
}
//---------------------------------

bool ResolverRequest::processSlot(int index)                // process the request into Sting format
{
    if(!slotIndexValid(index)) {                            // out of bounds? false
        return false;
    }

    Tresolv *r = &requests[index];

    if(!r->getaddrinfoHasFinished) {                        // if not done, or there is some error, fail
        return false;
    }

    if(r->error != EAI_INPROGRESS && r->error != 0) {       // if it's done, but there's some error, there's nothing to do here
        r->processed = 1;
        return true;
    }

    addrinfo *ai = r->req.ar_result;                        // get pointer to 1st result

    if(ai != NULL && ai->ai_canonname != NULL) {            // got adress info, and got cannonname? store it
        memset(r->canonName, 0, 256);
        strncpy(r->canonName, ai->ai_canonname, 255);
    }

    r->count = 0;   

    while(1) {
        if(ai == NULL) {
            break;
        }

        struct sockaddr *ai_addr = ai->ai_addr;             // treat as sockaddr struct

        if(ai_addr->sa_family == AF_INET) {                 // it's IPv4 addres? process it
            sockaddr_in * sa_in = (sockaddr_in *) ai_addr;  // cast to sockaddr_in
            DWORD ip            = sa_in->sin_addr.s_addr;   // get IP

            DWORD *pip = (DWORD *) r->data;
            bool foundIt = false;
            for(int i = 0; i < r->count; i++) {             // check if we don't have that IP stored already
                if(pip[i] == ip) {
                    foundIt = true;
                    break;
                }
            }

            if(!foundIt) {                                  // this IP not stored yet, store it
                pip[r->count] = ip;
                r->count++;
            }
        }

        ai = ai->ai_next;                                   // go to next result
    }

    r->processed = 1;
    return true;
}

void ResolverRequest::showSlot(int index)
{
    if(!slotIndexValid(index)) {        // out of bounds? false
        Debug::out(LOG_DEBUG, "resolveRequestShow - bad index");
        return;
    }

    Tresolv *r = &requests[index];

    if(!r->getaddrinfoHasFinished) {    // if not done, or there is some error, fail
        Debug::out(LOG_DEBUG, "resolveRequestShow - request not done yet");
        return;
    }

    if(!r->processed) {                 // not processed?
        Debug::out(LOG_DEBUG, "resolveRequestShow - not processed yet");
        return;
    }

    if(r->count > 0) {
        Debug::out(LOG_DEBUG, "[%d] host: %s (%s) resolved to %d IPs:", index, r->hostName, r->canonName, r->count);

        DWORD *pip = (DWORD *) r->data;
        for(int i=0; i < r->count; i++) {
            BYTE *ipb = (BYTE *) &pip[i];
            Debug::out(LOG_DEBUG, "IP %d: %d.%d.%d.%d", i, ipb[0], ipb[1], ipb[2], ipb[3]);
        }
    } else {
        Debug::out(LOG_DEBUG, "[%d] host: %s not resolved, error is: %d (%s)", index, r->hostName, r->error, gai_strerror(r->error));
    }

    Debug::out(LOG_DEBUG, "\n");
}

bool ResolverRequest::checkAndhandleSlot(int index)
{
    bool done = getaddrinfoHasFinished(index);  // check if the request is done
    
    if(!done) {
        return false;
    }

    if(!isProcessed(index)) {                   // if it's done, check if it's processed
        processSlot(index);                     // not processed, process it
    }
     
    return true;
}

void ResolverRequest::clearSlot(int index)
{
    if(!slotIndexValid(index)) {                // out of bounds? false
        return;
    }

    Tresolv *r = &requests[index];

    r->startTime                = 0;            // no start time
    r->getaddrinfoHasFinished   = 0;            // mark that we did not finish yet
    r->count                    = 0;            // no IPs stored yet
    r->processed                = 0;            // not processed yet
    memset(r->canonName, 0, 256);               // no canon name yet 
}

bool ResolverRequest::slotIndexValid(int index)
{
    if(index < 0 || index >= RESOLV_COUNT) {    // out of bounds? false
        return false;
    }

    return true;
}

//---------------------------------
