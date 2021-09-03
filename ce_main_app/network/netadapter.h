// vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab
#ifndef _NETADAPTER_H_
#define _NETADAPTER_H_

#include <unistd.h>

#include "../isettingsuser.h"
#include "resolver.h"
#include "readwrapper.h"
#include "icmpwrapper.h"

#include "sting.h"

class AcsiDataTrans;

#define NET_HANDLES_COUNT       32
#define NET_STARTING_HANDLE     0x50

#define network_handleIsValid(X)    ((X >= NET_STARTING_HANDLE) && (X < (NET_STARTING_HANDLE + NET_HANDLES_COUNT)))
#define network_slotIsValid(X)      ((X >= 0) && (X < (NET_STARTING_HANDLE + NET_HANDLES_COUNT)))

#define network_slotToHandle(X)     (X + NET_STARTING_HANDLE)
#define network_handleToSlot(X)	    (X - NET_STARTING_HANDLE)

//--------------------------------------------------------------------------

#define NET_BUFFER_SIZE     (1024 * 1024)
#define CON_BFR_SIZE        (100 * 1024)

//-------------------------------------
#define NETREQ_TYPE_RESOLVE     1

typedef struct {
    int type;
    
    std::string   strParam;
} TNetReq;

//-------------------------------------

class TNetConnection
{
public:
    TNetConnection() {                  // contructor to init stuff
        initVars();
    }

    ~TNetConnection() {                 // destructor to possibly close connection
        closeIt();
        readWrapper.clearAll();
    }

    void closeIt(void) {                // close the data socket
        if(fd != -1) {
            close(fd);
        }
        
        if(listenFd != -1) {            // close the listen socket
            close(listenFd);
        }

        initVars();
    }

    void cleanIt(void) {
        readWrapper.clearAll();
    }

    void initVars(void) {               // initialize the variables
        activeNotPassive    = true;
        fd                  = -1;
        listenFd            = -1;

        type                = 0;
        bytesInSocket       = 0;
        status              = TCLOSED;
        lastReadCount       = 0;
        
        memset(&remote_adr, 0, sizeof(remote_adr)); 
        localPort           = 0;

        gotPrevLastByte     = false;
        prevLastByte        = 0;
    }

    bool isClosed(void) {                       // check if it's closed
        if(fd == -1 && listenFd == -1) {        // normal and listen socket closed? is closed
            return true;
        }
        
        return false;                           // something is open
    }

    bool    activeNotPassive;       // socket type: active (outgoing) or passive (listening)
    int     fd;                     // file descriptor of socket
    int     listenFd;               // fd of listening socket
    struct  sockaddr_in remote_adr; // remote address
    uint16_t    localPort;              // local port
    int     type;                   // TCP / UDP / ICMP
    int     buff_size;              // for TCP - TX buffer size = maximu packet size that should be sent

    ReadWrapper readWrapper;

    int bytesInSocket;          // how many bytes are waiting to be read from socket
    int status;                 // status of connection - open, closed, ...
    int lastReadCount;          // count of bytes that was read on the last read operation

    bool gotPrevLastByte;       // flag that we do have a last byte from the previous transfer
    uint8_t prevLastByte;          // this is the last byte from previous transfer
};

//-------------------------------------

class NetAdapter: public ISettingsUser
{
public:
    NetAdapter(void);
    virtual ~NetAdapter();

    void reloadSettings(int type);
    void setAcsiDataTrans(AcsiDataTrans *dt);

    void processCommand(uint8_t *command);
	
private:
    uint8_t            *cmd;
    AcsiDataTrans   *dataTrans;
    uint8_t            *dataBuffer;

    TNetConnection  cons[NET_HANDLES_COUNT];    // for handling of TCP and UDP connections
    IcmpWrapper     icmpWrapper;                // for handling ICMP sending and receiving
    ResolverRequest resolver;                   // for handling DNS resolve requests
    
    void loadSettings(void);
    void identify(void);

    void conOpen(void);                 // TCP_open()  and UDP_open()     handling
    void conClose(void);                // TCP_close() and UDP_close()    handling
    void conSend(void);                 // TCP_send()  and UDP_send()     handling

    void conGetCharBuffer(void);        // CNget_char()  handling 
    void conGetNdb(void);               // CNget_NDB()   handling
    void conGetBlock(void);             // CNget_block() handling
    void conGetString(void);            // CNgets()      handling
    void conUpdateInfo(void);           // CNgetinfo() and CNbyte_count() handling

    void icmpSend(void);                // ICMP_send()   handling
    void icmpGetDgrams(void);           // ICMP datagram retrieving 

    void resolveStart(void);            // resolve name to ip
    void resolveGetResp(void);          // retrieve the results of resolve

    void conOpen_connect(int slot, bool tcpNotUdp, uint16_t localPort, uint32_t remoteHost, uint16_t remotePort, uint16_t tos, uint16_t buff_size);
    void conOpen_listen (int slot, bool tcpNotUdp, uint16_t localPort, uint32_t remoteHost, uint16_t remotePort, uint16_t tos, uint16_t buff_size);
    uint16_t getLocalPort(int sockFd);
    void setKeepAliveOptions(int fd);

    void updateCons_active (int i);
    void updateCons_passive(int i);
    bool didSocketHangUp   (int i);
    
    //--------------
    // helper functions
    int  findEmptyConnectionSlot(void); // get index of empty connection slot, or -1 if nothing is available
    void updateCons(void);

    void logFunctionName(uint8_t cmd);
    void closeAndCleanAll(void);
};

//-------------------------------------

#endif


