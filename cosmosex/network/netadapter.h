#ifndef _NETADAPTER_H_
#define _NETADAPTER_H_

#include <stdlib.h>
#include <string>
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

//-------------------------------------
#define STING_DGRAM_MAXSIZE     512

class TStingDgram {
public:
    TStingDgram() {
        data = new BYTE[STING_DGRAM_MAXSIZE];
        clear();
    }

    ~TStingDgram() {
        delete []data;
    }

    void clear(void) {
        memset(data, 0, STING_DGRAM_MAXSIZE);
        count   = 0;
        time    = 0;
    }

    bool isEmpty(void) {
        return (count == 0);
    }

    BYTE  *data;
    WORD  count;
    DWORD time;
};

//-------------------------------------

class TRawSocks{
public:
    TRawSocks() {
        packet      = (char*) new BYTE[ sizeof(struct iphdr) + sizeof(struct icmphdr) + CON_BFR_SIZE];

        ip          = (struct iphdr*)    packet;                                                    // pointer to IP   header (start of packet)
        icmp        = (struct icmphdr*) (packet + sizeof(struct iphdr));                            // pointer to ICMP header (that is just beyond IP header)
        data        = (BYTE *)          (packet + sizeof(struct iphdr) + sizeof(struct icmphdr));   // pointer to ICMP data   (located beyond ICMP header)
    }

    ~TRawSocks() {
        delete []packet;
    }

    void setIpHeader(DWORD src_addr, DWORD dst_addr, WORD dataLength) {
        ip->ihl         = 5;
        ip->version     = 4;        // IPv4
        ip->tos         = 0;
        ip->tot_len     = sizeof(struct iphdr) + sizeof(struct icmphdr) + dataLength;
        ip->id          = htons(random());
        ip->ttl         = 255;
        ip->protocol    = IPPROTO_ICMP;
        ip->saddr       = htonl(src_addr);
        ip->daddr       = htonl(dst_addr);

        ip->check       = 0;                                                // first set checksum to zero
        ip->check       = checksum((WORD *) ip, sizeof(struct iphdr));      // then calculate real checksum and store it
    }

    void setIcmpHeader(int type, int code, int id, int sequence) {
        icmp->type              = type;
        icmp->code              = code;
        icmp->un.echo.id        = htons(id);
        icmp->un.echo.sequence  = htons(sequence);

        icmp->checksum = 0;                                                 // first set checksum to zero
        icmp->checksum = checksum((WORD *) icmp, sizeof(struct icmphdr));   // then calculate real checksum and store it
        
        echoId = id;
    }

    static WORD checksum(WORD *addr, int len) {
        int sum         = 0;
        WORD answer  = 0;
        WORD *w      = addr;
        int nleft       = len;

        while (nleft > 1) {     // sum WORDs in a loop
            sum += *w++;
            nleft -= 2;
        }

        /* mop up an odd byte, if necessary */
        if (nleft == 1) {
            *(u_char *) (&answer) = *(u_char *) w;
            sum += answer;
        }

        /* add back carry outs from top 16 bits to low 16 bits */
        sum = (sum >> 16) + (sum & 0xffff); // add hi 16 to low 16
        sum += (sum >> 16);                 // add carry
        answer = ~sum;                      // truncate to 16 bits
        return (answer);
    }

    char*           packet;
    struct iphdr*   ip;
    struct icmphdr* icmp;
    BYTE*           data;
    
    WORD            echoId;
};

//-------------------------------------

class TNetConnection
{
public:
    TNetConnection() {                  // contructor to init stuff
        initVars();
    }

    ~TNetConnection() {                 // destructor to possibly close connection
        closeIt();
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
        
        memset(&remote_adr, '0', sizeof(remote_adr)); 
        memset(&local_adr, '0', sizeof(local_adr)); 

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
    struct  sockaddr_in local_adr;  // local  address
    int     type;                   // TCP / UDP / ICMP

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
    TNetConnection  rawSock;            // this is info about RAW socket - used for ICMP
    TRawSocks       rawSockHeads;       // this holds the headers for RAW socket

    BYTE *rBfr;                         // pointer to 100 kB read buffer - used when conLocateDelim() is called
    
    void loadSettings(void);

    void identify(void);

    void conOpen(void);                 // open connection
    void conClose(void);                // close connection            
    void conSend(void);                 // send data
    void conUpdateInfo(void);           // send connection info to ST
    void conReadData(void);             // receive data
    void conGetDataCount(void);         // get how many data there is
    void conLocateDelim(void);          // find string delimiter in received data

    void icmpSend(void);                // send ICMP packet
    void icmpGetDgrams(void);           // receive ICMP packets

    void resolveStart(void);            // resolve name to ip
    void resolveGetResp(void);          // retrieve the results of resolve

    void conOpen_connect(int slot, bool tcpNotUdp, DWORD remoteHost, WORD remotePort, WORD tos, WORD buff_size);
    void conOpen_listen(int slot, bool tcpNotUdp, WORD localPort, WORD tos, WORD buff_size);

    void updateCons_active(int i);
    void updateCons_passive(int i);
    bool didSocketHangUp(int i);
    
    //--------------
    // helper functions
    int  findEmptyConnectionSlot(void); // get index of empty connection slot, or -1 if nothing is available
    void updateCons(void);
    int  howManyWeCanReadFromFd(int fd);

    int  readFromSocket(TNetConnection *nc, int cnt);
    void finishDataRead(TNetConnection *nc, int totalCnt, BYTE status);
    
    void logFunctionName(BYTE cmd);
    void closeAndCleanAll(void);
};

//-------------------------------------

#endif


