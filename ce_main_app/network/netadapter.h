#ifndef _NETADAPTER_H_
#define _NETADAPTER_H_

#include <stdlib.h>
#include <string>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <linux/icmp.h>
#include <unistd.h>

#include "../acsidatatrans.h"
#include "../settings.h"
#include "../datatypes.h"
#include "../isettingsuser.h"
#include "resolver.h"
#include "readwrapper.h"
#include "icmpwrapper.h"

#include "sting.h"

#define MAX_HANDLE          32
#define NET_BUFFER_SIZE     (1024 * 1024)

#define CON_BFR_SIZE        (100 * 1024)

//-------------------------------------
#define NETREQ_TYPE_RESOLVE     1

typedef struct {
    int type;
    
    std::string   strParam;
} TNetReq;

//-------------------------------------

extern "C" {
	void netReqAdd(TNetReq &tnr);
	void *networkThreadCode(void *ptr);
}

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
    WORD    localPort;              // local port
    int     type;                   // TCP / UDP / ICMP
    int     buff_size;              // for TCP - TX buffer size = maximu packet size that should be sent

    ReadWrapper readWrapper;

    int bytesInSocket;          // how many bytes are waiting to be read from socket
    int status;                 // status of connection - open, closed, ...
    int lastReadCount;          // count of bytes that was read on the last read operation

    bool gotPrevLastByte;       // flag that we do have a last byte from the previous transfer
    BYTE prevLastByte;          // this is the last byte from previous transfer
};

//-------------------------------------

class NetAdapter: public ISettingsUser
{
public:
    NetAdapter(void);
    virtual ~NetAdapter();

    void reloadSettings(int type);
    void setAcsiDataTrans(AcsiDataTrans *dt);

    void processCommand(BYTE *command);
	
private:
    BYTE            *cmd;
    AcsiDataTrans   *dataTrans;
    BYTE            *dataBuffer;

    TNetConnection  cons[MAX_HANDLE];   // for handling of TCP and UDP connections
    IcmpWrapper     icmpWrapper;        // for handling ICMP sending and receiving
    ResolverRequest resolver;           // for handling DNS resolve requests
    
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

    void conOpen_connect(int slot, bool tcpNotUdp, WORD localPort, DWORD remoteHost, WORD remotePort, WORD tos, WORD buff_size);
    void conOpen_listen (int slot, bool tcpNotUdp, WORD localPort, DWORD remoteHost, WORD remotePort, WORD tos, WORD buff_size);

    void updateCons_active (int i);
    void updateCons_passive(int i);
    bool didSocketHangUp   (int i);
    
    //--------------
    // helper functions
    int  findEmptyConnectionSlot(void); // get index of empty connection slot, or -1 if nothing is available
    void updateCons(void);

    void logFunctionName(BYTE cmd);
    void closeAndCleanAll(void);
};

//-------------------------------------

#endif


