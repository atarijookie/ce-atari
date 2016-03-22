#ifndef _READWRAPPER_H_
#define _READWRAPPER_H_

#include <deque>

#define MAX_TOTAL_ITEMS_SIZE    (1024 * 1024)
#define TMP_BFR_SIZE            (  64 * 1024)

class NdbItem
{
public:
     NdbItem(int inSize, BYTE *inData, DWORD fromAddr, WORD fromPort);
    ~NdbItem(void);

    int   size;
    BYTE *data;

    struct {
        DWORD addr;
        WORD  port;
    } from;
};

class ReadWrapper 
{
public:
    ReadWrapper(void);
    ~ReadWrapper(void);

    void init(int inFd, int inType, int inBuffSize);
    void clearAll(void);

    int  getCount    (void);                         // get recv data count - for UDP this is merged count through datagrams

    int  getNdb        (BYTE *tmpBuffer);            // for UDP get one datagram, for TCP get a block from stream
    int  getNextNdbSize(void);
    
    int  peekBlock   (BYTE *tmpBuffer, int size);    // for UDP merge buffers, return data without removing from queue
    void removeBlock (int size);                     // remove from queue

private:
    int fd;             // file descriptor of socket
    int type;           // TCP / UDP / ICMP
    int buff_size;

    BYTE tmpBfr[TMP_BFR_SIZE];

    //----------------------------
    // for UDP
    void udpTryReceive    (void);
    int  getItemsTotalSize(void);
    void prunItemsIfNeeded(void);
    std::deque<NdbItem *> items;

    //----------------------------
    // for TCP
    int  tcpBytesWaiting  (void);
};

#endif

