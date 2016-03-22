#include <string.h>
#include <stdio.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h> 
#include <fcntl.h>
#include <poll.h>

#include <signal.h>
#include <pthread.h>
#include <queue>    

#include "../utils.h"
#include "../global.h"
#include "../debug.h"

#include "netadapter.h"
#include "netadapter_commands.h"
#include "sting.h"
#include "readwrapper.h"

//---------------------------------------------
NdbItem::NdbItem(int inSize, BYTE *inData, DWORD fromAddr, WORD fromPort)
{
    data = new BYTE[size];                  // allocate RAM
    size = size;

    memcpy(data, inData, inSize);           // copy in data

    from.addr = fromAddr;                   // store address
    from.port = fromPort;
}

NdbItem::~NdbItem(void)
{
    delete []data;
}

//---------------------------------------------
ReadWrapper::ReadWrapper(void)
{
    init(0, 0, 0);
}

ReadWrapper::~ReadWrapper(void)
{
    clearAll();
}

void ReadWrapper::init(int inFd, int inType, int inBuffSize)
{
    this->fd            = inFd;
    this->type          = inType;
    this->buff_size     = inBuffSize;

    clearAll();
}

void ReadWrapper::clearAll(void)
{
    while(items.size() > 0) {                           // if there are some items
        NdbItem *item = items.front();                  // get item
        delete item;                                    // delete it
        items.pop_front();                              // remove it from deque
    }
}

int ReadWrapper::getCount(void)                         // get recv data count - for UDP this is merged count through datagrams
{
    int totalCount = 0;

    if(fd < 1) {                                        // invalid handle? nothing to read
        return 0;
    }

    //--------------
    // for TCP
    if(type == TCP) {                                   // for TCP - use ioctl() to find count of bytes in stream
        totalCount = tcpBytesWaiting();
        return totalCount;
    }

    //--------------
    // for UDP
    if(type == UDP) {
        udpTryReceive();                                // try to receive if something is waiting

        totalCount = getItemsTotalSize();               // return how much data we have
        return totalCount;
    }

    //--------------
    // otherwise
    return 0;
}

int ReadWrapper::getNdb(BYTE *tmpBuffer)                // for UDP get one datagram, for TCP get a block from stream
{
    int res;
    int size = getNextNdbSize();                        // find out how much we should get

    if(size < 1) {                                      // nothing? quit
        return 0;
    }

    //--------------
    // for TCP
    if(type == TCP) {
        res = recv(fd, tmpBuffer, size, MSG_DONTWAIT); // read the data
        return res;
    }

    //--------------
    // for UDP
    if(type == UDP) {
        udpTryReceive();                                // try to receive if something is waiting

        NdbItem *item = items.front();                  // get item
        memcpy(tmpBuffer, item->data, size);            // copy the data

        delete item;                                    // delete it
        items.pop_front();                              // remove pointer from deque

        return size;
    }

    //--------------
    // otherwise
    return 0;
}

int ReadWrapper::getNextNdbSize(void)
{
    int nextSize;

    //--------------
    // for TCP
    if(type == TCP) {
        int totalCount  = tcpBytesWaiting();            // how many bytes are waiting?
        nextSize        = MIN(totalCount, buff_size);

        return nextSize;
    }

    //--------------
    // for UDP
    if(type == UDP) {
        udpTryReceive();                                // try to receive if something is waiting

        NdbItem *item = items.front();                  // get item
        nextSize = MIN(item->size, 32*1024);

        return nextSize;
    }

    //--------------
    // otherwise
    return 0;
}


int ReadWrapper::peekBlock(BYTE *tmpBuffer, int size)   // for UDP merge buffers, return data without removing from queue
{
    int totalCount, res;

    //--------------
    // for TCP
    if(type == TCP) {
        totalCount = tcpBytesWaiting();                            // how many bytes are waiting?

        size = MIN(totalCount, size);
        res  = recv(fd, tmpBuffer, size, MSG_DONTWAIT | MSG_PEEK); // peek the data (leave it in deque)

        return res;
    }

    //--------------
    // for UDP
    if(type == UDP) {
        udpTryReceive();                                // try to receive if something is waiting

        BYTE *p       = tmpBuffer;
        int itemsCnt  = items.size();                   // how many items we have? 
        int remaining = size;                           // how many bytes we still have to receive

        // merge received datagrams into single block
        for(int i=0; i<itemsCnt; i++) {
            NdbItem *item = items[i];                   // get item

            if(item->size <= remaining) {               // if the whole item will fit in the rest of the buffer, use whole item
                memcpy(p, item->data, item->size);

                remaining   -= item->size;
                p           += item->size;
            } else {                                    // if this item won't fit in the rest of buffer, use just part of the item and quit
                memcpy(p, item->data, remaining);
                remaining    = 0;
                break;
            }
        }

        return (size - remaining);                      // return how many bytes we got
    }

    //--------------
    // otherwise
    return 0;
}

void ReadWrapper::removeBlock(int size)                 // remove from queue
{
    int totalCount, res;

    //--------------
    // for TCP
    if(type == TCP) {
        totalCount = tcpBytesWaiting();                     // how many bytes are waiting?

        int howManyWeWillRemove = MIN(totalCount, size);    // how many we will remove?

        while(1) {                                          // remove data in a loop, remove by the size of TMP BUFFER
            int removeCountSingle = (howManyWeWillRemove < TMP_BFR_SIZE) ? howManyWeWillRemove : TMP_BFR_SIZE;

            res = recv(fd, tmpBfr, removeCountSingle, MSG_DONTWAIT);    // get the data (remove it)

            if(res < 0) {
                break;
            }

            howManyWeWillRemove -= removeCountSingle;       // update count how many we still need to remove
        }

        return;
    }

    //--------------
    // for UDP
    if(type == UDP) {
        udpTryReceive();                                // try to receive if something is waiting

        int itemsCnt  = items.size();                   // how many items we have? 
        int remaining = size;                           // how many bytes we still need to remove?

        // merge received datagrams into single block
        for(int i=0; i<itemsCnt; i++) {
            NdbItem *item = items.front();              // get item

            if(remaining <= 0) {                        // nothing to remove? quit
                break;
            }

            if(item->size <= remaining) {               // if the whole item needs to be removed, remove it
                delete item;
                items.pop_front();                

                remaining -= item->size;
            } else {                                    // if only part of this item needs to be removed, remove just part and quit
                int restThatStaysAfterRemove = item->size - remaining;

                memcpy(tmpBfr, item->data + remaining,  restThatStaysAfterRemove);      // copy the rest to tmp buffer  
                memcpy(item->data, tmpBfr,              restThatStaysAfterRemove);      // now back from tmp buffer to item
                item->size = restThatStaysAfterRemove;                                  // store the new size
                break;
            }
        }

        return;
    }
}

//-------------------------------
int ReadWrapper::tcpBytesWaiting(void)
{
    int res, value;

    res = ioctl(fd, FIONREAD, &value);              // try to get how many bytes can be read
    if(res < 0) {                                   // ioctl failed? 
        value = 0;
    }

    return value;
}

//-------------------------------
void ReadWrapper::udpTryReceive(void)
{
    int res, readCount;

    struct sockaddr_in si_other;
           int         slen = sizeof(si_other);

    int loops = 1000;
    while(loops > 0) {
        loops--;

        res = ioctl(fd, FIONREAD, &readCount);  // try to get how many bytes can be read
        if(res < 0) {                           // ioctl failed? nothing more to receive
            break;
        }

        // limit how much we can receive and receive it
        readCount = (readCount < TMP_BFR_SIZE) ? readCount : TMP_BFR_SIZE;

        res = recvfrom(fd, tmpBfr, readCount, 0, (struct sockaddr *) &si_other, (socklen_t *) &slen);

        if(res < 0) {                           // failed to receive? skip the rest
            continue;
        }

        // put it in the deque
        NdbItem *newItem = new NdbItem(readCount, tmpBfr, (DWORD) si_other.sin_addr.s_addr, (WORD) si_other.sin_port);
        items.push_back(newItem);
    }

    prunItemsIfNeeded();                        // remove items if they take too much place in RAM
}

int ReadWrapper::getItemsTotalSize(void)
{
    int totalSize = 0;
    int count = items.size();

    int i;
    for(i=0; i<count; i++) {
        totalSize += items[i]->size;
    }

    return totalSize;
}

void ReadWrapper::prunItemsIfNeeded(void)
{
    while(1) {
        int totalSize = getItemsTotalSize();        // calculate size of all items

        if(totalSize < MAX_TOTAL_ITEMS_SIZE) {      // if less than allowed, OK, can quit
            break;
        }

        NdbItem *item = items.back();               // get item
        delete item;                                // delete it
        items.pop_back();                           // remove pointer from deque
    }
}




