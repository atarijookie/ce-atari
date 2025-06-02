#ifndef __CHIPINTERFACENETWORK_H__
#define __CHIPINTERFACENETWORK_H__

#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_CLIENTS     8

class Ikbd;

class ChipInterfaceNetwork
{
public:
    ChipInterfaceNetwork();
    virtual ~ChipInterfaceNetwork();

    //----------------
    // chip interface initialization and deinitialization - e.g. open GPIO, or open socket, ...
    bool ciOpen(void);
    void ciClose(void);

    int getFdListen(void);
    void acceptSocketIfNeededAndPossible(void);

    int disconnectInactiveClients(void);
    int setAllClientFds(fd_set* readfds);
    void handleAllReadyClients(bool skipKeyboardTranslation, fd_set* readfds, Ikbd* ikbd);

    void ikbdUartWriteToAll(uint8_t* bfr, int len);

private:
    int fdListen;                       // socket for listen()
    int fdClients[MAX_CLIENTS];         // socket received on accept()
    uint32_t clientLastMs[MAX_CLIENTS]; // value of getCurrentMs() when was last time anything was received from this client

    struct sockaddr_in addressListen;
    struct sockaddr_in addressReport;

    void createListeningSocket(void);
    int getEmptyClientIndex(void);
    uint32_t recvFromClient(uint8_t* buf, int maxLen);
};

#endif // __CHIPINTERFACENETWORK_H__
