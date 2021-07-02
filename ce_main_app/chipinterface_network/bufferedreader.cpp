#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../utils.h"
#include "../datatypes.h"
#include "bufferedreader.h"

BufferedReader::BufferedReader()
{
    fd = -1;
    gotBytes = 0;
    txLen = 0;
    rxLen = 0;
    remainingPacketLength = 0;
}

BufferedReader::~BufferedReader()
{

}

void BufferedReader::setFd(int inFd)
{
    fd = inFd;
}

int BufferedReader::waitForATN(BYTE atnCode, DWORD timeoutMs, BYTE *inBuf)
{
    if(fd <= 0) {                   //  no fd, no ATN
        return NET_ATN_NONE_ID;
    }

    bool waitedOnce = false;                                // when timeoutMs says don't wait for data, make it wait once for a short time to be able to detect client socket disconnect
    uint32_t endTime = Utils::getEndTime(timeoutMs);        // when this time is reached and no data is available, quit

    while(sigintReceived == 0) {
        // got data? process
        if(gotBytes >= 12) {    // have enough data?
            int atnId = readHeaderFromBuffer(atnCode);

            if(atnId != NET_ATN_NONE_ID) {                  // if valid ATN ID found and header seems to be OK, return that ATN ID
                return atnId;
            }

            // no known tag string or header invalid? remove 1 byte from begining, start loop over again
            popFirst();
            continue;
        } 

        // Not enough data in buffer?
        // check if data was received and is present in socket by using ioctl().
        // if data is  available, the rest does: ioctl()            + recv()
        // if data not available, the rest does: ioctl() + select() + recv()

        // we need 12 bytes - 4 are the ATN STR, 8 are header (0xcafe, 0, ATN code, txLen, rxLen)
        int needCnt = 12 - gotBytes;

        int res, bytesAvailable;
        res = ioctl(fd, FIONREAD, &bytesAvailable);     // how many bytes we can read immediately?

        if(res < 0) {                                   // ioctl() failed? no bytes available
            bytesAvailable = 0;
        }

        if(bytesAvailable <= 0) {                       // if no bytes available? wait at least once
            uint32_t now = Utils::getCurrentMs();
            uint32_t timeLeftUs = (endTime > now) ? ((endTime - now) * 1000) : 0;   // if still not timeout, calculate how much time is left in us; otherwise no time left

            if(timeoutMs == 0 && !waitedOnce) {         // if this is the wait for 1st ATN (with 0 wait time), but didn't wait at least once, wait at least once for a short time
                waitedOnce = true;                      // now were about to wait once
                timeLeftUs = 1000;                      // just a short 1 ms wait
            }

            if(timeLeftUs <= 0) {                       // no time left? quit loop, return NONE ATN
                break;
            }

            struct timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = timeLeftUs;               // set timeout in us

            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(fd, &readfds);

            res = select(fd + 1, &readfds, NULL, NULL, &timeout);     // wait for data or timeout here

            if(res < 0 || !FD_ISSET(fd, &readfds)) {    // if select() failed or cannot read from fd, skip rest
                continue;
            }
        }

        // If got here, then either ioctl() told us we got some bytes available,
        // or select() told us we can recv() from socket now.

        ssize_t recvCnt = recv(fd, &buffer[gotBytes], needCnt, 0);

        if(recvCnt == 0) {                              // if recv() returned 0, then client disconnected
            return NET_ATN_DISCONNECTED;
        }

        if(recvCnt > 0) {                               // if read was OK, we got those bytes and we can restart the loop
            gotBytes += recvCnt;
            continue;
        }

        // on failed to get data code continues here, try the loop again
    }

    return NET_ATN_NONE_ID;     // nothing usable found
}

void BufferedReader::popFirst(void)
{
    if(gotBytes < 1) {              // no data? nothing to do
        return;
    }

    gotBytes--;                     // we got now 1 byte less

    int i;
    for(i=0; i<gotBytes; i++) {     // move bytes to lower index
        buffer[i] = buffer[i + 1];
    }
}

int BufferedReader::readHeaderFromBuffer(uint8_t atnCodeWant)
{
    // the buffer should contain: 
    //  0...3: ATN tag (4 bytes)
    //  4...5: 0xcafe (2 bytes)
    //  6...7: ATN code/command (2 bytes)
    //  8...9: txLen (2 bytes)
    // 10..11: rxLen (2 bytes) 
    // total: 12 bytes

    if(gotBytes < 12) {                 // should have enough data to check them
        return NET_ATN_NONE_ID;
    }

    int atnId = NET_ATN_NONE_ID;

    if(memcmp(buffer, NET_ATN_HANS_STR, 4) == 0) {  // ATN Hans string?
        atnId = NET_ATN_HANS_ID;
    }

    if(memcmp(buffer, NET_ATN_FRANZ_STR, 4) == 0) {  // ATN Franz string?
        atnId = NET_ATN_FRANZ_ID;
    }

    if(memcmp(buffer, NET_ATN_IKBD_STR, 4) == 0) {  // ATN IKBD string?
        atnId = NET_ATN_IKBD_ID;
    }

    if(memcmp(buffer, NET_ATN_ZEROS_STR, 4) == 0) {  // ATN Zeros string?
        atnId =  NET_ATN_ZEROS_ID;
    }

    if(atnId == NET_ATN_NONE_ID) {      // no valid ATN STR tag found? quit
        return NET_ATN_NONE_ID;
    }

    uint16_t syncWord = Utils::getWord(&buffer[4]);

    if(syncWord != 0xcafe) {            // sync word is not 0xcafe
        return NET_ATN_NONE_ID;
    }

    if(atnId == NET_ATN_HANS_ID || atnId == NET_ATN_FRANZ_ID) {     // if it's for Hans or Franz, possibly check ATN code (for ZEROS and IKBD don't check it)
        if(atnCodeWant != ATN_ANY) {                                // if it's not this special value, we're waiting for specific ATN code
            uint8_t atnCodeGot = getAtnCode();

            if(atnCodeGot != atnCodeWant) {                         // the ATN code (command) is wrong, fail
                return NET_ATN_NONE_ID;
            }
        }
    }

    // read TX and RX length in words
    txLen = Utils::getWord(&buffer[8]);
    rxLen = Utils::getWord(&buffer[10]);

    // WORD count to BYTE count
    txLen *= 2;
    rxLen *= 2;

    // remove 8 bytes from the length, as we've already read this 8 byte long headers
    txLen = (txLen >= 8) ? (txLen - 8) : 0;
    rxLen = (rxLen >= 8) ? (rxLen - 8) : 0;

    remainingPacketLength = MAX(txLen, rxLen);  // the remaining length is the bigger one out of TX and RX length

    // value other than NET_ATN_NONE_ID means success
    return atnId;
}

// from the current buffer gets and returns the ATN code found in header
uint8_t BufferedReader::getAtnCode(void)
{
    return buffer[7];
}

uint16_t BufferedReader::getRemainingLength(void)
{
    return remainingPacketLength;
}

// pointer to header start in our buffer
uint8_t* BufferedReader::getHeaderPointer(void)
{
    return &buffer[4];
}

// clear buffer after reading valid header
void BufferedReader::clear(void)
{
    gotBytes = 0;               // don't have any bytes anymore
    memset(buffer, 0, 12);
}
