#ifndef _BUFFEREDREADER_H_
#define _BUFFEREDREADER_H_

#include <stdint.h>
#include "../chipinterface.h"

// ATN tags which define who is sending the data (Hans, Franz, IKBD, or zero padding)
#define NET_ATN_HANS_STR    "ATHA"
#define NET_ATN_FRANZ_STR   "ATFR"
#define NET_ATN_IKBD_STR    "ATIK"
#define NET_ATN_ZEROS_STR   "ATZE"

#define NET_ATN_NONE_ID         0
#define NET_ATN_HANS_ID         1
#define NET_ATN_FRANZ_ID        2
#define NET_ATN_IKBD_ID         3
#define NET_ATN_ZEROS_ID        4
#define NET_ATN_DISCONNECTED    0xee
#define NET_ATN_ANY_ID          0xff

class BufferedReader
{
public:
    BufferedReader();
    ~BufferedReader();

    // set file descriptor for reading
    void setFd(int inFd);
    
    // clear buffer after reading valid header
    void clear(void);

    // reads (and waits for) data from socket and if valid header found, returns one of the NET_ATN_*_ID codes or NET_ATN_NONE_ID if header not received
    int waitForATN(uint8_t atnCode, uint32_t timeoutMs);

    // from the current buffer gets and returns the ATN code found in header
    uint8_t getAtnCode(void);

    // pointer to header start in our buffer
    uint8_t* getHeaderPointer(void);

    // how many bytes we should read to read this ATN command completely?
    uint16_t getRemainingLength(void);

private:
    int fd;

    uint8_t buffer[32];
    int     gotBytes;

    uint16_t txLen;
    uint16_t rxLen;
    uint16_t remainingPacketLength;

    void popFirst(void);
    int readHeaderFromBuffer(uint8_t atnCodeWant);
};

#endif // _BUFFEREDREADER_H_
