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
	
//private:
    AcsiDataTrans   *dataTrans;
    BYTE            *cmd;

    BYTE            *dataBuffer;

    TNetConnection  cons[MAX_HANDLE];   // this holds the info about connections

    BYTE *rBfr;                         // pointer to 100 kB read buffer - used when conLocateDelim() is called

    ResolverRequest resolver;           // DNS resolver
    
    void loadSettings(void);

    void identify(void);

    void conOpen(void);                 // open connection
    void conClose(void);                // close connection            
    void conSend(void);                 // send data
    void conUpdateInfo(void);           // send connection info to ST
    void conReadData(void);             // receive data
    void conGetDataCount(void);         // get how many data there is
    void conLocateDelim(void);          // find string delimiter in received data

    void conGetCharBuffer(void);
    void conGetNdb(void);
    void conGetBlock(void);
    void conGetString(void);

    void icmpSend(void);                // send ICMP packet
    void icmpGetDgrams(void);           // receive ICMP packets

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

    int  readFromSocket(TNetConnection *nc, int wantCount);
    void finishDataRead(TNetConnection *nc, int totalCnt, BYTE status);
    
    void logFunctionName(BYTE cmd);
    void closeAndCleanAll(void);
};

//-------------------------------------

#endif


