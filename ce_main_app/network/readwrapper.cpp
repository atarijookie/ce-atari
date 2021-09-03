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
NdbItem::NdbItem(int inSize, uint8_t *inData, uint32_t fromAddr, uint16_t fromPort)
{
    data = new uint8_t[inSize];                // allocate RAM
    memcpy(data, inData, inSize);           // copy in data

    size = inSize;
    
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
        if(items.empty()) {                             // nothing in deque? quit
            return;
        }

        NdbItem *item = (NdbItem *) items.front();      // get item
        
        if(item == NULL) {
            continue;
        }

        delete item;                                    // delete it
        items.pop_front();                              // remove it from deque
    }
}

int ReadWrapper::getCount(void)                         // get recv data count - for UDP this is merged count through datagrams
{
    int totalCount = 0;

    if(fd < 1) {                                        // invalid handle? nothing to read
        Debug::out(LOG_DEBUG, "ReadWrapper::getCount() - invalid handle, returned 0");
        return 0;
    }

    //--------------
    // for TCP
    if(type == TCP) {                                   // for TCP - use ioctl() to find count of bytes in stream
        totalCount = tcpBytesWaiting();
        Debug::out(LOG_DEBUG, "ReadWrapper::getCount() - TCP connection has %d bytes", totalCount);
        return totalCount;
    }

    //--------------
    // for UDP
    if(type == UDP) {
        udpTryReceive();                                // try to receive if something is waiting

        totalCount = getItemsTotalSize();               // return how much data we have
        Debug::out(LOG_DEBUG, "ReadWrapper::getCount() - UDP connection has %d bytes", totalCount);
        return totalCount;
    }

    //--------------
    // otherwise
    return 0;
}

int ReadWrapper::getNdb(uint8_t *tmpBuffer)                // for UDP get one datagram, for TCP get a block from stream
{
    int res;
    int size = getNextNdbSize();                        // find out how much we should get

    if(size < 1) {                                      // nothing? quit
        Debug::out(LOG_DEBUG, "ReadWrapper::getNdb() - next NDB too small, quit");
        return 0;
    }

    //--------------
    // for TCP
    if(type == TCP) {
        res = recv(fd, tmpBuffer, size, MSG_DONTWAIT); // read the data
        Debug::out(LOG_DEBUG, "ReadWrapper::getNdb() - TCP recv: %d", res);
		//Debug::outBfr(tmpBuffer, res);
        return res;
    }

    //--------------
    // for UDP
    if(type == UDP) {
        udpTryReceive();                                // try to receive if something is waiting

        if(items.empty()) {                             // nothing in deque? quit
            return 0;
        }
        
        NdbItem *item = (NdbItem *) items.front();      // get item

        if(item == NULL) {
            return 0;
        }

        memcpy(tmpBuffer, item->data, size);            // copy the data

        delete item;                                    // delete it
        items.pop_front();                              // remove pointer from deque

        Debug::out(LOG_DEBUG, "ReadWrapper::getNdb() - UDP size: %d", size);
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

        if(items.empty()) {                             // nothing in deque? quit
            return 0;
        }
        
        NdbItem *item = (NdbItem *) items.front();      // get item
        
        if(item == NULL) {
            return 0;
        }
        
        nextSize = MIN(item->size, 32*1024);

        return nextSize;
    }

    //--------------
    // otherwise
    return 0;
}


int ReadWrapper::peekBlock(uint8_t *tmpBuffer, int size)   // for UDP merge buffers, return data without removing from queue
{
    int totalCount, res;

	if(size <= 0) {
		return 0;	//nothing to do
	}

    if(type == TCP) {        //-------------- for TCP
        totalCount = tcpBytesWaiting();                            // how many bytes are waiting?
		if(totalCount <= 0) {
	        Debug::out(LOG_DEBUG, "ReadWrapper::peekBlock() - TCP No available byte. asked %d", size);
			return 0;
		}

        size = MIN(totalCount, size);
        res  = recv(fd, tmpBuffer, size, MSG_DONTWAIT | MSG_PEEK); // peek the data (leave it in deque)

		if(res < 0) {
			if(errno == EAGAIN || errno == EWOULDBLOCK) {
				return 0;	// nothing available yet
			}
			Debug::out(LOG_ERROR, "ReadWrapper::peekBlock() - TCP recv() %d bytes error : %s", size, strerror(errno));
		} else {
	        Debug::out(LOG_DEBUG, "ReadWrapper::peekBlock() - TCP peek %d bytes, res: %d", size, res);
		}
        return res;
    } else if(type == UDP) { //-------------- for UDP
        udpTryReceive();                                // try to receive if something is waiting

        if(items.empty()) {                             // nothing in deque? quit
            Debug::out(LOG_DEBUG, "ReadWrapper::peekBlock() - UDP peek - nothing in queue");
            return 0;
        }
        
        uint8_t *p       = tmpBuffer;
        int itemsCnt  = items.size();                   // how many items we have? 
        int remaining = size;                           // how many bytes we still have to receive
        int ndbUsed   = 0;        
        
        // merge received datagrams into single block
        for(int i=0; i<itemsCnt; i++) {
            NdbItem *item = (NdbItem *) items[i];       // get item
            ndbUsed++;
            
            if(item->size <= remaining) {               // if the whole item will fit in the rest of the buffer, use whole item
                memcpy(p, item->data, item->size);

                remaining   -= item->size;
                p           += item->size;
                
                if(remaining <= 0) {
                    break;
                }                
            } else {                                    // if this item won't fit in the rest of buffer, use just part of the item and quit
                memcpy(p, item->data, remaining);
                remaining    = 0;
                break;
            }
        }

        int returnedCount = size - remaining;
        Debug::out(LOG_DEBUG, "ReadWrapper::peekBlock() - UDP peek %d bytes, used %d NDBs", returnedCount, ndbUsed);
        return returnedCount;                       // return how many bytes we got
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

        while(howManyWeWillRemove > 0) {                    // remove data in a loop, remove by the size of TMP BUFFER
            int removeCountSingle = MIN(howManyWeWillRemove, TMP_BFR_SIZE);

            res = recv(fd, tmpBfr, removeCountSingle, MSG_DONTWAIT);    // get the data (remove it)
            if(res < 0) {
				Debug::out(LOG_ERROR, "ReadWrapper::removeBlock() TCP recv error : %s", strerror(errno));
                break;
            }
			Debug::out(LOG_DEBUG, "ReadWrapper::removeBlock() TCP removed %d bytes", res);
            howManyWeWillRemove -= res;       // update count how many we still need to remove
        }

        return;
    }

    //--------------
    // for UDP
    if(type == UDP) {
        udpTryReceive();                                // try to receive if something is waiting

        if(items.empty()) {                             // nothing in deque? quit
            return;
        }
        
        int itemsCnt    = items.size();                 // how many items we have? 
        int remaining   = size;                         // how many bytes we still need to remove?
        int ndbRemoved  = 0;
        bool lastPartial = false;

        // merge received datagrams into single block
        for(int i=0; i<itemsCnt; i++) {
            if(remaining <= 0) {                        // nothing to remove? quit
                break;
            }
        
            ndbRemoved++;
            NdbItem *item = (NdbItem *) items.front();  // get item

            if(item == NULL) {
                break;
            }
            
            if(item->size <= remaining) {               // if the whole item needs to be removed, remove it
                Debug::out(LOG_DEBUG, "ReadWrapper::removeBlock(%d), loop %d - item.size() is %d bytes, removing whole block", size, i, item->size);
                lastPartial = false;                    // full block remove

                remaining -= item->size;                // subtract size

                delete item;                            // delete item
                items.pop_front();                
                
                if(remaining <= 0) {
                    break;
                }
            } else {                                    // if only part of this item needs to be removed, remove just part and quit
                Debug::out(LOG_DEBUG, "ReadWrapper::removeBlock(%d), loop %d - item.size() is %d bytes, removing part of the block", size, i, item->size);
                lastPartial = true;                     // partial block remove

                int restThatStaysAfterRemove = item->size - remaining;

                memcpy(tmpBfr, item->data + remaining,  restThatStaysAfterRemove);      // copy the rest to tmp buffer  
                memcpy(item->data, tmpBfr,              restThatStaysAfterRemove);      // now back from tmp buffer to item
                item->size = restThatStaysAfterRemove;                                  // store the new size
                break;
            }
        }

        Debug::out(LOG_DEBUG, "ReadWrapper::removeBlock(%d) - UDP removed %d bytes (%d remaining), removed %d NDBs, last was %s NDB remove", size, (size - remaining), remaining, ndbRemoved, (lastPartial ? "partial" : "full"));
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

        res = recvfrom(fd, tmpBfr, readCount, MSG_DONTWAIT, (struct sockaddr *) &si_other, (socklen_t *) &slen);

        if(res < 0) {                           // failed to receive? skip the rest
            if(errno == EAGAIN || errno == EWOULDBLOCK) {   // no data? quit loop
                break;
            }
        	Debug::out(LOG_ERROR, "ReadWrapper::udpTryReceive() recvfrom() %d bytes error : %s", readCount, strerror(errno));
            continue;                                       // other error? try again
        }

        Debug::out(LOG_DEBUG, "ReadWrapper::udpTryReceive() - readCount: %d, addr: %08x, port: %d", readCount, ntohl(si_other.sin_addr.s_addr), ntohs(si_other.sin_port));
        
        // put it in the deque
        NdbItem *newItem = new NdbItem(readCount, tmpBfr, (uint32_t) si_other.sin_addr.s_addr, (uint16_t) si_other.sin_port);
        items.push_back(newItem);
    }

    prunItemsIfNeeded();                        // remove items if they take too much place in RAM
}

int ReadWrapper::getItemsTotalSize(void)
{
    if(items.empty()) {                             // nothing in deque? quit
        return 0;
    }

    int totalSize = 0;
    int count = items.size();

    int i;
    for(i=0; i<count; i++) {
        NdbItem *item;
        item        = (NdbItem *) items[i];
        totalSize  += item->size;
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

        if(items.empty()) {                         // nothing in deque? quit
            return;
        }

        NdbItem *item = (NdbItem *) items.back();   // get item

        if(item == NULL) {
            break;
        }
        
        delete item;                                // delete it
        items.pop_back();                           // remove pointer from deque
    }
}
