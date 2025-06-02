#ifndef __CHIPINTERFACENETWORK_H__
#define __CHIPINTERFACENETWORK_H__

#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>

class ChipInterfaceNetwork
{
public:
    ChipInterfaceNetwork();
    virtual ~ChipInterfaceNetwork();

    //----------------
    // chip interface initialization and deinitialization - e.g. open GPIO, or open socket, ...
    bool ciOpen(void);
    void ciClose(void);

    void ikbdUartEnable(bool enable);
    int  ikbdUartReadFd(void);
    int  ikbdUartWriteFd(void);

private:
    uint32_t lastTimeRecv;

    int fdListen;       // socket for listen()
    int fdClient;       // socket received on accept()

    struct sockaddr_in addressListen;
    struct sockaddr_in addressReport;

    void createListeningSocket(void);
    void acceptSocketIfNeededAndPossible(void);
    void closeClientSocket(void);
    uint32_t recvFromClient(uint8_t* buf, int maxLen);
};

#endif // __CHIPINTERFACENETWORK_H__
