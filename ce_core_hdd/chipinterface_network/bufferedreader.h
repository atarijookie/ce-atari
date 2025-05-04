#ifndef _BUFFEREDREADER_H_
#define _BUFFEREDREADER_H_

#include <stdint.h>
#include "../chipinterface.h"

#define NET_ATN_NONE_ID         0
#define NET_ATN_HANS_ID         1
#define NET_ATN_FRANZ_ID        2
#define NET_ATN_IKBD_ID         3
#define NET_ATN_DISCONNECTED    0xee
#define NET_ATN_ANY_ID          0xff

#define HEADER_BUFFER_SIZE  10

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
    int waitForAtn(uint8_t atnCode, uint32_t timeoutMs);

    // from the current buffer gets and returns the ATN code found in header
    uint8_t getAtnCode(void);

    // pointer to header start in our buffer
    uint8_t* getHeaderPointer(void);

    // how many bytes we should read to read this ATN command completely?
    uint32_t dataSizeRest(void);
    
    // after reading only part of the data, use this method to decrease the remaining size
    void decreaseDataSize(uint32_t decreaseBy);

private:
    int fd;

    uint8_t buffer[HEADER_BUFFER_SIZE];
    int     gotBytes;

    uint32_t dataSizeBytes;

    void popFirst(void);
    int readHeaderFromBuffer(uint8_t atnCodeWant);
};

#endif // _BUFFEREDREADER_H_
