// vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab
#ifndef _READWRAPPER_H_
#define _READWRAPPER_H_

#define MAX_TOTAL_ITEMS_SIZE    (1024 * 1024)
#define TMP_BFR_SIZE            (  64 * 1024)

class NdbItem
{
public:
     NdbItem(int inSize, uint8_t *inData, uint32_t fromAddr, uint16_t fromPort);
    ~NdbItem(void);

    int   size;
    uint8_t *data;

    struct {
        uint32_t addr;
        uint16_t  port;
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

    int  getNdb        (uint8_t *tmpBuffer);            // for UDP get one datagram, for TCP get a block from stream
    int  getNextNdbSize(void);
    
    int  peekBlock   (uint8_t *tmpBuffer, int size);    // for UDP merge buffers, return data without removing from queue
    void removeBlock (int size);                     // remove from queue

private:
    int fd;             // file descriptor of socket
    int type;           // TCP / UDP / ICMP
    int buff_size;

    uint8_t tmpBfr[TMP_BFR_SIZE];

    //----------------------------
    // for UDP
    void udpTryReceive    (void);
    int  getItemsTotalSize(void);
    void prunItemsIfNeeded(void);

    //----------------------------
    // for TCP
    int  tcpBytesWaiting  (void);
};

#endif
