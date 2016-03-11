#include <string.h>
#include <stdio.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h> 
#include <fcntl.h>
#include <poll.h>

#include <signal.h>
#include <pthread.h>
#include <queue>    

#include "../utils.h"
#include "../global.h"
#include "../debug.h"

#include "netadapter.h"
#include "netadapter_commands.h"
#include "sting.h"

#define REQUIRED_NETADAPTER_VERSION     0x0100

//--------------------------------------------------------

pthread_mutex_t networkThreadMutex = PTHREAD_MUTEX_INITIALIZER;
std::queue<TNetReq> netReqQueue;

DWORD localIp;
int   rawSockFd = -1;
bool  tryIcmpRecv(BYTE *bfr);

void resolveNameToIp(TNetReq &tnr);

#define RECV_BFR_SIZE   (64 * 1024)

#define MAX_STING_DGRAMS    32
TStingDgram dgrams[MAX_STING_DGRAMS];
volatile DWORD icmpDataCount;

void dgram_clearOld(void) 
{
    DWORD now = Utils::getCurrentMs();
    
    for(int i=0; i<MAX_STING_DGRAMS; i++) {     // find non-empty slot, and if it's old, clear it
        if(dgrams[i].isEmpty()) {               // found empty? skip it
            continue;
        }
        
        DWORD diff = now - dgrams[i].time;      // calculate how old is this dgram
        if(diff < 10000) {                      // dgram is younger than 10 seconds? skip it
            continue;
        }

        dgrams[i].clear();                      // it's too old, clear it
        Debug::out(LOG_DEBUG, "dgram_clearOld() - dgram #%d was too old and it was cleared", i);
    }
}

int dgram_getEmpty(void) {
    int i; 
    DWORD oldestTime    = 0xffffffff;
    int   oldestIndex   = 0;
    
    for(i=0; i<MAX_STING_DGRAMS; i++) {     // try to find empty slot
        if(dgrams[i].time < oldestTime) {   // found older item than we had found before? store index
            oldestTime  = dgrams[i].time;
            oldestIndex = i;
        }
    
        if(dgrams[i].isEmpty()) {           // found empty? return it
            return i;
        }
    }

    // no empty slot found, clear and return the oldest - to avoid filling up the dgrams array (at the cost of loosing oldest items)
    Debug::out(LOG_DEBUG, "dgram_getEmpty() - no empty slot, returning oldest slot - #%d", oldestIndex);
    
    dgrams[oldestIndex].clear();
    return oldestIndex;
}

int dgram_getNonEmpty(void) {
    int i; 
    for(i=0; i<MAX_STING_DGRAMS; i++) {
        if(!dgrams[i].isEmpty()) {
            return i;
        }
    }

    return -1;
}

int dgram_calcIcmpDataCount(void) {
    DWORD sum = 0;
    int i; 

    for(i=0; i<MAX_STING_DGRAMS; i++) {     // go through received DGRAMs
        if(!dgrams[i].isEmpty()) {          // not empty?
            sum += dgrams[i].count;         // add size of this DGRAM
        }
    }

    return sum;
}

void netReqAdd(TNetReq &tnr)
{
	pthread_mutex_lock(&networkThreadMutex);            // try to lock the mutex
	netReqQueue.push(tnr);                              // add this to queue
	pthread_mutex_unlock(&networkThreadMutex);          // unlock the mutex
}

void *networkThreadCode(void *ptr)
{
	Debug::out(LOG_DEBUG, "Network thread starting...");
    BYTE *recvBfr = new BYTE[RECV_BFR_SIZE];                // 64 kB receive buffer    

    // The following line is needed to allow root to create raw sockets for ICMP echo... 
    // The problem with this is it allows only ICMP echo to be done.
    system("sysctl -w net.ipv4.ping_group_range=\"0 0\" > /dev/null");  

	while(sigintReceived == 0) {
        dgram_clearOld();                               // clear old dgrams that are probably stuck in the queue 
    
        while(1) {                                      // receive all available ICMP data 
            bool r = tryIcmpRecv(recvBfr);              // this will cause 100 ms delay when no data is there
            if(!r) {                                    // if receiving failed, quit; otherwise do another receiving!
                break;
            }
        }                           

		pthread_mutex_lock(&networkThreadMutex);		// lock the mutex

		if(netReqQueue.size() == 0) {					// nothing to do?
			pthread_mutex_unlock(&networkThreadMutex);	// unlock the mutex
			Utils::sleepMs(100); 						// wait 100 ms and try again
			continue;
		}
		
		TNetReq tnr = netReqQueue.front();		        // get the 'oldest' element from queue
		netReqQueue.pop();								// and remove it form queue
		pthread_mutex_unlock(&networkThreadMutex);		// unlock the mutex


    }

    delete []recvBfr;
	
	Debug::out(LOG_DEBUG, "Network thread terminated.");
	return 0;
}

//--------------------------------------------------------
bool tryIcmpRecv(BYTE *bfr)
{
    if(rawSockFd == -1) {                                   // ICMP socket closed? quit, no data
        return false;
    }

    //-----------------------
    // wait for data for 100 ms, and quit if there is no data
    struct timeval  timeout = {0, 100000};                  // receive timeout - 100 ms
    fd_set          read_set;

    memset(&read_set, 0, sizeof(read_set));
    FD_SET(rawSockFd, &read_set);

    int res = select(rawSockFd + 1, &read_set, NULL, NULL, &timeout);     //wait for a reply with a timeout

    if(res == 0) {                  // no fd? timeout, quit
        return false;
    }
    
    if(res < 0) {                   // select failed
        return false;
    }

    //-----------------------
    // receive the data
    // recvfrom will receive only one ICMP packet, even if there are more than 1 packets waiting in socket
    struct sockaddr src_addr;
    int addrlen = sizeof(struct sockaddr);

    res = recvfrom(rawSockFd, bfr, RECV_BFR_SIZE, 0, (struct sockaddr *) &src_addr, (socklen_t *) &addrlen);

    if(res == -1) {                 // if recvfrom failed, no data
        Debug::out(LOG_DEBUG, "tryIcmpRecv - recvfrom() failed");
        return false;
    }

    // res now contains length of ICMP packet (header + data)

    //-----------------------
    // parse response to the right structs
    pthread_mutex_lock(&networkThreadMutex);

    int i = dgram_getEmpty();       // now find space for the datagram
    if(i == -1) {                   // no space? fail, but return that we were able to receive data
        pthread_mutex_unlock(&networkThreadMutex);
        
        Debug::out(LOG_DEBUG, "tryIcmpRecv - dgram_getEmpty() returned -1");
        return true;
    }

    TStingDgram *d = &dgrams[i];
    d->clear();
    d->time = Utils::getCurrentMs();
    
    //-------------
    // fill IP header
    d->data[0] = 0x45;                          // IP ver, IHL
    Utils::storeWord(d->data + 2, 20 + res);    // data[2 .. 3] = TOTAL LENGTH = IP header lenght (20) + ICMP header & data length (res)
    d->data[8] = 128;                           // TTL
    d->data[9] = ICMP;                          // protocol

    struct sockaddr_in *sa_in = (struct sockaddr_in *) &src_addr;       // cast sockaddr to sockaddr_in
    Utils::storeDword(d->data + 12, ntohl(sa_in->sin_addr.s_addr));     // data[12 .. 15] - source IP
    Utils::storeDword(d->data + 16, localIp);                           // data[16 .. 19] - destination IP 

    WORD checksum = TRawSocks::checksum((WORD *) d->data, 20);
    Utils::storeWord(d->data + 10, checksum);                           // calculate chekcsum, store to data[10 .. 11]
    //-------------
    // fill IP_DGRAM header
    Utils::storeWord(d->data + 30, res);                                // data[30 .. 31] - pkt_length - length of IP packet data block (res)
    Utils::storeDword(d->data + 32, 128);                               // data[32 .. 35] - timeout - timeout of packet life

    //-------------
    // now append ICMP packet

    int rest = MIN(STING_DGRAM_MAXSIZE - 48 - 2, res);                  // we can store only (STING_DGRAM_MAXSIZE - 48 - 2) = 462 bytes of ICMP packets to fit 512 B
    memcpy(d->data + 48, bfr, rest);                                    // copy whole ICMP packet beyond the IP_DGRAM structure

    //--------------
    // epilogue - update stuff, unlock mutex, success!
    d->count = 48 + rest;                                               // update how many bytes this gram contains all together

    icmpDataCount = dgram_calcIcmpDataCount();                          // update icmpDataCount

    Debug::out(LOG_DEBUG, "tryIcmpRecv - icmpDataCount is now %d bytes", icmpDataCount);
    pthread_mutex_unlock(&networkThreadMutex);
    return true;
}

//--------------------------------------------------------

NetAdapter::NetAdapter(void)
{
    dataTrans       = 0;
    dataBuffer      = new BYTE[NET_BUFFER_SIZE];
    localIp         = 0;
    icmpDataCount   = 0;

    rBfr = new BYTE[CON_BFR_SIZE];
    memset(rBfr, 0, CON_BFR_SIZE);
    
    loadSettings();
}

NetAdapter::~NetAdapter()
{
    delete []dataBuffer;
    delete []rBfr;
}

void NetAdapter::setAcsiDataTrans(AcsiDataTrans *dt)
{
    dataTrans = dt;
}

void NetAdapter::reloadSettings(int type)
{
    loadSettings();
}

void NetAdapter::loadSettings(void)
{
    Debug::out(LOG_DEBUG, "NetAdapter::loadSettings");

    // first read the new settings
    Settings s;


}

void NetAdapter::processCommand(BYTE *command)
{
    cmd = command;

    if(dataTrans == 0) {
        Debug::out(LOG_ERROR, "NetAdapter::processCommand was called without valid dataTrans, can't tell ST that his went wrong...");
        return;
    }

    dataTrans->clear();                 // clean data transporter before handling

    BYTE *pCmd;
    BYTE isIcd = false;

    isIcd   = ((command[0] & 0x1f) == 0x1f);            // it's an ICD command, if lowest 5 bits are all set in the cmd[0]
    pCmd    = (!isIcd) ? command : (command + 1);       // get the pointer to where the command starts

    // pCmd[1] & pCmd[2] are 'CE' tag, pCmd[3] is host module ID
    // pCmd[4] will be command, the rest will be params - depending on command type
    
    logFunctionName(pCmd[4]);
    
    switch(pCmd[4]) {
        case NET_CMD_IDENTIFY:              identify();         break;

        // TCP functions
        case NET_CMD_TCP_OPEN:              conOpen();          break;
        case NET_CMD_TCP_CLOSE:             conClose();         break;
        case NET_CMD_TCP_SEND:              conSend();          break;
        case NET_CMD_TCP_WAIT_STATE:        break;                      // currently not used on host
        case NET_CMD_TCP_ACK_WAIT:          break;                      // currently not used on host
        case NET_CMD_TCP_INFO:              break;                      // currently not used on host

        // UDP FUNCTION
        case NET_CMD_UDP_OPEN:              conOpen();          break;
        case NET_CMD_UDP_CLOSE:             conClose();         break;
        case NET_CMD_UDP_SEND:              conSend();          break;

        // ICMP FUNCTIONS
        case NET_CMD_ICMP_SEND_EVEN:        icmpSend();         break;
        case NET_CMD_ICMP_SEND_ODD:         icmpSend();         break;
        case NET_CMD_ICMP_HANDLER:          break;                      // currently not used on host
        case NET_CMD_ICMP_DISCARD:          break;                      // currently not used on host
        case NET_CMD_ICMP_GET_DGRAMS:       icmpGetDgrams();    break;

        // CONNECTION MANAGER
        case NET_CMD_CNKICK:                break;                      // currently not used on host
        case NET_CMD_CNBYTE_COUNT:          break;                      // currently not used on host
        case NET_CMD_CNGET_CHAR:            break;                      // currently not used on host
        case NET_CMD_CNGET_NDB:             break;                      // currently not used on host
        case NET_CMD_CNGET_BLOCK:           break;                      // currently not used on host
        case NET_CMD_CNGETINFO:             break;                      // currently not used on host
        case NET_CMD_CNGETS:                break;                      // currently not used on host
        case NET_CMD_CN_UPDATE_INFO:        conUpdateInfo();    break;
        case NET_CMD_CN_READ_DATA:          conReadData();      break;
        case NET_CMD_CN_GET_DATA_COUNT:     conGetDataCount();  break;
        case NET_CMD_CN_LOCATE_DELIMITER:   conLocateDelim();   break;

        // MISC
        case NET_CMD_RESOLVE:               resolveStart();     break;
        case NET_CMD_RESOLVE_GET_RESPONSE:  resolveGetResp();   break;
        case NET_CMD_ON_PORT:               break;                      // currently not used on host
        case NET_CMD_OFF_PORT:              break;                      // currently not used on host
        case NET_CMD_QUERY_PORT:            break;                      // currently not used on host
        case NET_CMD_CNTRL_PORT:            break;                      // currently not used on host
    }

    dataTrans->sendDataAndStatus();     // send all the stuff after handling, if we got any
}
//----------------------------------------------
void NetAdapter::identify(void)
{
    //--------
    // as this happens on the start of fake STiNG driver, do some clean up from previous driver run
    
    closeAndCleanAll();
    //--------
    dataTrans->addDataBfr((BYTE *) "CosmosEx network module", 24, false);   // add 24 bytes which are the identification string
    
    BYTE bfr[10];
    memset(bfr, 0, 8);
    dataTrans->addDataBfr(bfr, 8, false);                                   // add 8 bytes of padding, so the config data would be at offset 32
    
    //---------
    // now comes the config, starting at offset 32
    dataTrans->addDataWord(REQUIRED_NETADAPTER_VERSION);                    // add WORD - protocol version

    Utils::getIpAdds(bfr);                          // get IP address for eth0 and wlan0

    if(bfr[0] == 1) {                               // eth0 enabled? add its IP                         
        dataTrans->addDataBfr(bfr + 1, 4, false);
        localIp = Utils::getDword(bfr + 1);         // store eth0 as local ip
    } else if(bfr[5] == 1) {                        // wlan0 enabled? add its IP
        dataTrans->addDataBfr(bfr + 6, 4, false);
        localIp = Utils::getDword(bfr + 6);         // store wlan0 as local ip
    } else {                                        // eth0 and wlan0 disabled? send zeros
        memset(bfr, 0, 4);
        dataTrans->addDataBfr(bfr, 4, false);
        localIp = 0;                                // no local IP
    }

    //--------
    // now finish the data and status byte
    dataTrans->padDataToMul16();                    // make sure the data size is multiple of 16
    dataTrans->setStatus(E_NORMAL);
}
//----------------------------------------------
void NetAdapter::closeAndCleanAll(void)
{
    pthread_mutex_lock(&networkThreadMutex);        // try to lock the mutex

    int i;
    for(i=0; i<MAX_HANDLE; i++) {                   // close normal sockets
        cons[i].closeIt();
    }
    
    rawSock.closeIt();                              // close raw / icmp socket
    
    for(i=0; i<MAX_STING_DGRAMS; i++) {             // clear received icmp dgrams
        dgrams[i].clear();
    }
    
	pthread_mutex_unlock(&networkThreadMutex);      // unlock the mutex
}
//----------------------------------------------
void NetAdapter::conOpen(void)
{
    bool tcpNotUdp;

    // get type of connection
    if(cmd[4] == NET_CMD_TCP_OPEN) {
        tcpNotUdp = true;
    } else if(cmd[4] == NET_CMD_UDP_OPEN) {
        tcpNotUdp = false;
    } else {
        dataTrans->setStatus(E_PARAMETER);
        return;
    }

    bool res = dataTrans->recvData(dataBuffer, 512);    // get data from Hans

    if(!res) {                                          // failed to get data? internal error!
        Debug::out(LOG_DEBUG, "NetAdapter::conOpen - failed to receive data...");
        dataTrans->setStatus(E_PARAMETER);
        return;
    }

    int slot = findEmptyConnectionSlot();               // try to find empty slot
    
    if(slot == -1) {
        Debug::out(LOG_DEBUG, "NetAdapter::conOpen - no more free handles");
        dataTrans->setStatus(E_CONNECTFAIL);
        return;
    }

    // get connection parameters
    DWORD remoteHost        = Utils::getDword(dataBuffer);
    WORD  remotePort        = Utils::getWord (dataBuffer + 4);
    bool  connectNotListen  = (remoteHost != 0);
        
    Debug::out(LOG_DEBUG, "NetAdapter::conOpen() -- remoteHost: %d.%d.%d.%d, remotePort: %d -- will %s", (BYTE) (remoteHost >> 24), (BYTE) (remoteHost >> 16), (BYTE) (remoteHost >> 8), (BYTE) (remoteHost), remotePort, (connectNotListen ? "connect to host" : "listen for connection"));
    
    // following 2 params can be received, but are not used for now
    WORD  tos           = Utils::getWord (dataBuffer + 6);
    WORD  buff_size     = Utils::getWord (dataBuffer + 8);
    
    if(remoteHost != 0) {       // remote host specified? Open by connecting to remote host
        conOpen_connect(slot, tcpNotUdp, remoteHost, remotePort, tos, buff_size);
    } else {                    // remote host not specified? Open by listening for connection
        conOpen_listen(slot, tcpNotUdp, remotePort, tos, buff_size);
    }
}

void NetAdapter::conOpen_listen(int slot, bool tcpNotUdp, WORD localPort, WORD tos, WORD buff_size)
{
    int ires;
    int fd;
    
    if(tcpNotUdp) {     // TCP connection
        fd = socket(AF_INET, SOCK_STREAM, 0);
    } else {            // UDP connection
        fd = socket(AF_INET, SOCK_DGRAM, 0);
    }
    
    if(fd < 0) {        // failed? quit
        Debug::out(LOG_DEBUG, "NetAdapter::conOpen_listen - socket() failed");
        dataTrans->setStatus(E_CONNECTFAIL);
        return;
    }
    
    if(tcpNotUdp) {                                                 // for TCP sockets
        int flags = fcntl(fd, F_GETFL, 0);                          // get flags
        ires = fcntl(fd, F_SETFL, flags | O_NONBLOCK);              // set it as non-blocking

        if(ires == -1) {
            Debug::out(LOG_DEBUG, "NetAdapter::conOpen_listen - setting O_NONBLOCK failed, but continuing");
        }
    }

    // fill the local address info
    struct sockaddr_in local_addr;
    memset(&local_addr, '0', sizeof(local_addr)); 
    local_addr.sin_family        = AF_INET;
    local_addr.sin_addr.s_addr   = htonl(INADDR_ANY);
    local_addr.sin_port          = htons(localPort); 
    
    ires = bind(fd, (struct sockaddr*) &local_addr, sizeof(local_addr));  // bind IP & port to this socket
    
    if(ires == -1) {                                                    // if bind failed, quit
        close(fd);
        
        Debug::out(LOG_DEBUG, "NetAdapter::conOpen_listen - bind() failed with error: %d", errno);
        dataTrans->setStatus(E_CONNECTFAIL);
        return;
    }
    
    if(localPort == 0) {                                                // should listen on 1st free port?
        struct sockaddr_in real_addr;
        memset(&real_addr, '0', sizeof(local_addr)); 

        socklen_t len = (socklen_t) sizeof(real_addr);
        ires = getsockname(fd, (struct sockaddr*) &real_addr, &len);    // try to find out real local port number that was open / used
        
        if(ires == -1) {
            close(fd);

            Debug::out(LOG_DEBUG, "NetAdapter::conOpen_listen - getsockname() failed with error: %d", errno);
            dataTrans->setStatus(E_CONNECTFAIL);
            return;
        }
        
        local_addr.sin_port = real_addr.sin_port;                   // store port
        WORD port = ntohs(local_addr.sin_port);
        Debug::out(LOG_DEBUG, "NetAdapter::conOpen_listen - now listening on first free port %d (hex: 0x%04x, dec: %d, %d)", port, port, port >> 8, port & 0xff);
    } else {
        Debug::out(LOG_DEBUG, "NetAdapter::conOpen_listen - now listening on specified port %d (hex: 0x%04x, dec: %d, %d)", localPort, localPort, localPort >> 8, localPort & 0xff);
    }
    
    listen(fd, 1);                                                  // mark this socket as listening, with queue length of 1
    
    cons[slot].initVars();                                          // init vars

    // store the info
    cons[slot].activeNotPassive = false;                            // it's passive (listening) socket
    cons[slot].listenFd         = fd;
    cons[slot].local_adr        = local_addr;
    cons[slot].type             = tcpNotUdp ? TCP : UDP;
    cons[slot].bytesInSocket    = 0;
    cons[slot].status           = TLISTEN;

    // return the handle
    dataTrans->setStatus(handleAtariToCE(slot));
}

void NetAdapter::conOpen_connect(int slot, bool tcpNotUdp, DWORD remoteHost, WORD remotePort, WORD tos, WORD buff_size)
{
    int ires;
    int fd;
    
    if(tcpNotUdp) {     // TCP connection
        fd = socket(AF_INET, SOCK_STREAM, 0);
    } else {            // UDP connection
        fd = socket(AF_INET, SOCK_DGRAM, 0);
    }

    if(fd < 0) {        // failed? quit
        Debug::out(LOG_DEBUG, "NetAdapter::conOpen_connect - socket() failed");
        dataTrans->setStatus(E_CONNECTFAIL);
        return;
    }

    // fill the remote host info
    struct sockaddr_in serv_addr;
    memset(&serv_addr, '0', sizeof(serv_addr)); 
    serv_addr.sin_family        = AF_INET;
    serv_addr.sin_addr.s_addr   = htonl(remoteHost);
    serv_addr.sin_port          = htons(remotePort); 

    if(tcpNotUdp) {                                             // for TCP sockets
        int flags = fcntl(fd, F_GETFL, 0);                      // get flags
        ires = fcntl(fd, F_SETFL, flags | O_NONBLOCK);          // set it as non-blocking

        if(ires == -1) {
            Debug::out(LOG_DEBUG, "NetAdapter::conOpen_connect - setting O_NONBLOCK failed, but continuing");
        }
    }

    // for TCP - try to connect, for UPD - set the default address where to send data
    ires = connect(fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));  

    if(ires < 0 && errno != EINPROGRESS) {      // if connect failed, and it's not EINPROGRESS (because it's O_NONBLOCK)
        close(fd);

        Debug::out(LOG_DEBUG, "NetAdapter::conOpen_connect - connect() failed");
        dataTrans->setStatus(E_CONNECTFAIL);
        return;
    }

    int conStatus = TESTABLISH;                 // for UDP and blocking TCP, this is 'we have connection'
    if(ires < 0 && errno == EINPROGRESS) {      // if it's a O_NONBLOCK socket connecting, the state is connecting
        conStatus = TSYN_SENT;                  // for non-blocking TCP, this is 'we're trying to connect'
        Debug::out(LOG_DEBUG, "NetAdapter::conOpen_connect() - non-blocking TCP is connecting, going to TSYN_SENT status");
    }
    
    cons[slot].initVars();                      // init vars

    // store the info
    cons[slot].activeNotPassive = true;         // it's active (outgoing) socket
    cons[slot].fd               = fd;
    cons[slot].remote_adr       = serv_addr;
    cons[slot].type             = tcpNotUdp ? TCP : UDP;
    cons[slot].bytesInSocket    = 0;
    cons[slot].status           = conStatus;

    // return the handle
    dataTrans->setStatus(handleAtariToCE(slot));
}
//----------------------------------------------
void NetAdapter::conClose(void)
{
    bool res = dataTrans->recvData(dataBuffer, 512);    // get data from Hans

    if(!res) {                                          // failed to get data? internal error!
        Debug::out(LOG_DEBUG, "NetAdapter::conClose - failed to receive data...");
        dataTrans->setStatus(E_PARAMETER);
        return;
    }

    int handle = Utils::getWord(dataBuffer);            // retrieve handle

    if(handle < 0 || handle >= MAX_HANDLE) {            // handle out of range? fail
        Debug::out(LOG_DEBUG, "NetAdapter::conClose() -- bad handle: %d", handle);
        dataTrans->setStatus(E_PARAMETER);
        return;
    }

    if(cons[handle].isClosed()) {                       // handle already closed? fail
        Debug::out(LOG_DEBUG, "NetAdapter::conClose() -- handle %d is already closed, pretending that it was closed :)", handle);
        dataTrans->setStatus(E_NORMAL);
        return;
    }

    Debug::out(LOG_DEBUG, "NetAdapter::conClose() -- closing connection %d", handle);
    cons[handle].closeIt();                             // handle good, close it

    dataTrans->setStatus(E_NORMAL);
}
//----------------------------------------------
void NetAdapter::conSend(void)
{
    int  cmdType    = cmd[4];                           // command type: NET_CMD_TCP_SEND or NET_CMD_UDP_SEND
    int  handle     = cmd[5];                           // connection handle
    int  length     = Utils::getWord(cmd + 6);          // get data length
    bool isOdd      = cmd[8];                           // if the data was send from odd address, this will be non-zero...
    BYTE oddByte    = cmd[9];                           // ...and this will contain the 0th byte

    if(handle < 0 || handle >= MAX_HANDLE) {            // handle out of range? fail
        Debug::out(LOG_DEBUG, "NetAdapter::conSend() -- bad handle: %d", handle);
        dataTrans->setStatus(E_BADHANDLE);
        return;
    }

    if(cons[handle].isClosed()) {                       // connection not open? fail
        Debug::out(LOG_DEBUG, "NetAdapter::conSend() -- connection %d is closed", handle);
        dataTrans->setStatus(E_BADHANDLE);
        return;
    }

    bool good = false;                                  // check if trying to do right type of send over right type of connection (TCP over TCP, UDP over UDP)
    if( (cmdType == NET_CMD_TCP_SEND && cons[handle].type == TCP) || 
        (cmdType == NET_CMD_UDP_SEND && cons[handle].type == UDP)) {
        good = true;
    }

    if(!good) {                                         // send type vs. connection type mismatch? fail
        Debug::out(LOG_DEBUG, "NetAdapter::conSend() -- bad combination of connection type and command");
        dataTrans->setStatus(E_BADHANDLE);
        return;
    }

    int lenRoundUp;
    if((length % 512) == 0) {                           // length of data is a multiple of 512? good
        lenRoundUp = length;
    } else {                                            // length of data is not a multiple of 512? Let's fix this
        lenRoundUp = ((length / 512) + 1) * 512;        // get sector count, then increment it by 1, and convert back to bytes - this will round the length to next multiple of 512
    }

    BYTE *pData;
    if(isOdd) {                                         // if we're transfering from odd ST address
        dataBuffer[0]   = oddByte;                      // then this is the 0th byte
        pData           = dataBuffer + 1;               // and the rest of data will be transfered here
    } else {                                            // id we're transfering from even ST address
        pData           = dataBuffer;                   // all the data will be transfered here
    }

    bool res = dataTrans->recvData(pData, lenRoundUp);  // get data from Hans

    if(!res) {                                          // failed to get data? internal error!
        Debug::out(LOG_DEBUG, "NetAdapter::conSend - failed to receive data...");
        dataTrans->setStatus(E_PARAMETER);
        return;
    }

    Debug::out(LOG_DEBUG, "NetAdapter::conSend() -- sending %d bytes through connection %d (received %d from ST, isOdd: %d)", length, handle, lenRoundUp, isOdd);
    Debug::outBfr(pData, length);
    
    int ires = write(cons[handle].fd, dataBuffer, length);  // try to send the data

    if(ires < length) {                                 // if written less than should, fail
        Debug::out(LOG_DEBUG, "NetAdapter::conSend - failed to write() all data");
        dataTrans->setStatus(E_OBUFFULL);
        return;
    }

    dataTrans->setStatus(E_NORMAL);
}
//----------------------------------------------
void NetAdapter::conUpdateInfo(void)
{
    int i;

    updateCons();                                           // update connections status and bytes to read

    int found = 0;
    
    // fill the buffer
    for(i=0; i<MAX_HANDLE; i++) {                           // store how many bytes we can read from connections
        DWORD bytesToBeRead = cons[i].bytesInSocket;
        dataTrans->addDataDword(bytesToBeRead);
        
        if(bytesToBeRead > 0) {
            Debug::out(LOG_DEBUG, "NetAdapter::conUpdateInfo - connection %d has %d bytes waiting to be read", i, bytesToBeRead);
            found++;
        }
    }

    for(i=0; i<MAX_HANDLE; i++) {                           // store connection statuses
        dataTrans->addDataByte(cons[i].status);
    }

    for(i=0; i<MAX_HANDLE; i++) {                           // store local ports (LPort)
        dataTrans->addDataWord(ntohs(cons[i].local_adr.sin_port));
    }

    for(i=0; i<MAX_HANDLE; i++) {                           // store remote addresses (RHost)
        dataTrans->addDataDword(ntohl(cons[i].remote_adr.sin_addr.s_addr));
    }

    for(i=0; i<MAX_HANDLE; i++) {                           // store remote ports (RPort)
        dataTrans->addDataWord(ntohs(cons[i].remote_adr.sin_port));
    }

    pthread_mutex_lock(&networkThreadMutex);
    dataTrans->addDataDword(icmpDataCount);                 // fill the data to be read from ICMP sock
    pthread_mutex_unlock(&networkThreadMutex);

    Debug::out(LOG_DEBUG, "NetAdapter::conUpdateInfo - icmpDataCount: %d", icmpDataCount);

    dataTrans->padDataToMul16();
    dataTrans->setStatus(E_NORMAL);
}
//----------------------------------------------
void NetAdapter::conReadData(void)
{
    int   handle                = cmd[5];                       // get handle
    DWORD byteCountStRequested  = Utils::get24bits(cmd + 6);    // get how many bytes we want to read
    int   seekOffset            = (signed char) cmd[9];         // get seek offset (can be 0 or -1)

    if(handle < 0 || handle >= MAX_HANDLE) {                    // handle out of range? fail
        Debug::out(LOG_DEBUG, "NetAdapter::conReadData -- bad handle: %d", handle);
        dataTrans->setStatus(E_BADHANDLE);
        return;
    }

    TNetConnection *nc = &cons[handle];                         // will use this nc instead of longer cons[handle] in the next lines

    if(nc->isClosed()) {                                        // connection not open? fail
        Debug::out(LOG_DEBUG, "NetAdapter::conReadData -- connection %d is closed", handle);
        dataTrans->setStatus(E_BADHANDLE);
        return;
    }

    Debug::out(LOG_DEBUG, "NetAdapter::conReadData -- handle: %d, byteCountStRequested: %d, seekOffset: %d", handle, byteCountStRequested, seekOffset);
    
    int totalCnt    = 0;
    int toRead      = byteCountStRequested;

    //--------------
    // first handle negative seek offset
    if(seekOffset == -1 && nc->gotPrevLastByte) {               // if we want to seek one byte back, and we do have that byte, do seek
        dataTrans->addDataByte(nc->prevLastByte);               // add this byte as first
        nc->gotPrevLastByte = false;

        totalCnt++;
        toRead--;

        if(toRead == 0) {                                       // if this is what ST wanted to read, quit
            Debug::out(LOG_DEBUG, "NetAdapter::conReadData -- finished with SINGLE byte returned, totalCnt: %d bytes", totalCnt);
            
            finishDataRead(nc, totalCnt, RW_ALL_TRANSFERED);    // update variables, set status
            return;
        }
    }

    //--------------
    // now find out if we can read anything
    nc->bytesInSocket = howManyWeCanReadFromFd(nc->fd);                     // update how many bytes we have waiting in socket

    if(nc->bytesInSocket == 0) {                                            // nothing to read? return that we don't have enough data
        Debug::out(LOG_DEBUG, "NetAdapter::conReadData -- not enough data, RW_PARTIAL_TRANSFER, totalCnt: %d bytes", totalCnt);

        finishDataRead(nc, totalCnt, RW_PARTIAL_TRANSFER);                  // update variables, set status
        return;
    }

    //-----------------
    // try to read the data from socket
    int cntSck = readFromSocket(nc, toRead);

    toRead      -= cntSck;
    totalCnt    += cntSck;

    if(toRead == 0) {                                               // we don't need to read more? quit with success
        Debug::out(LOG_DEBUG, "NetAdapter::conReadData -- finishing after readFromSocked() with RW_ALL_TRANSFERED, totalCnt: %d bytes", totalCnt);
        
        finishDataRead(nc, totalCnt, RW_ALL_TRANSFERED);            // transfered all that was wanted
    } else {
        Debug::out(LOG_DEBUG, "NetAdapter::conReadData -- finishing after readFromSocked() with RW_PARTIAL_TRANSFER, totalCnt: %d bytes", totalCnt);

        finishDataRead(nc, totalCnt, RW_PARTIAL_TRANSFER);          // transfer was just partial
    }
}
//----------------------------------------------
void NetAdapter::conGetDataCount(void)
{
    int handle = cmd[5];                                    // get handle

    if(handle < 0 || handle >= MAX_HANDLE) {                // handle out of range? fail
        dataTrans->setStatus(E_BADHANDLE);
        return;
    }

    if(cons[handle].isClosed()) {                           // connection not open? fail
        dataTrans->setStatus(E_BADHANDLE);
        return;
    }

    dataTrans->addDataDword(cons[handle].lastReadCount);    // store last read count
    dataTrans->setStatus(E_NORMAL);
}
//----------------------------------------------
void NetAdapter::conLocateDelim(void)
{
    int  handle = cmd[5];                               // get connection handle
    BYTE delim  = cmd[6];                               // get string delimiter
    
    if(handle < 0 || handle >= MAX_HANDLE) {            // handle out of range? fail
        Debug::out(LOG_DEBUG, "NetAdapter::conLocateDelim -- bad handle: %d", handle);
        dataTrans->setStatus(E_BADHANDLE);
        return;
    }

    TNetConnection *nc = &cons[handle];

    if(nc->isClosed()) {                                        // connection not open? fail
        Debug::out(LOG_DEBUG, "NetAdapter::conLocateDelim -- connection %d closed", handle);
        dataTrans->setStatus(E_BADHANDLE);
        return;
    }

    //-----------------
    // if there's some data we can read, PEEK data (leave it in socket for next real read)
    int canRead     = howManyWeCanReadFromFd(nc->fd);                   // find out how many bytes we can read
    
    if(canRead <= 0) {
        Debug::out(LOG_DEBUG, "NetAdapter::conLocateDelim -- delimiter NOT found, because no data to recv");
        dataTrans->addDataDword(DELIMITER_NOT_FOUND);
        return;
    }
    
    int readCount   = MIN(canRead, CON_BFR_SIZE);                       // get the smaller number out of these two

    int res = recv(nc->fd, rBfr, readCount, MSG_DONTWAIT | MSG_PEEK);   // PEEK read the data

    if(res <= 0) {                                                      // read failed? quit
        Debug::out(LOG_DEBUG, "NetAdapter::conLocateDelim -- delimiter NOT found, because recv failed");
        dataTrans->addDataDword(DELIMITER_NOT_FOUND);
        return;
    }
    //-----------------
    // now try to find the delimiter
    int i;
    bool found = false;
    for(i=0; i<res; i++) {                              // go through the buffer and find delimiter
        if(rBfr[i] == delim) {                          // delimiter found?
            found = true;
            break;
        }
    }

    if(found) {                                         // if found, store position
        Debug::out(LOG_DEBUG, "NetAdapter::conLocateDelim -- delimiter found at position %d", i);
        dataTrans->addDataDword(i);
    } else {                                            // not found? return DELIMITER_NOT_FOUND
        Debug::out(LOG_DEBUG, "NetAdapter::conLocateDelim -- delimiter NOT found");
        dataTrans->addDataDword(DELIMITER_NOT_FOUND);
    }
    dataTrans->padDataToMul16();                        // make sure the data size is multiple of 16

    dataTrans->setStatus(E_NORMAL);                     // this operation was OK
}
//----------------------------------------------
void NetAdapter::icmpSend(void)
{
    bool evenNotOdd;

    if(cmd[4] == NET_CMD_ICMP_SEND_EVEN) {
        evenNotOdd = true;
    } else if(cmd[4] == NET_CMD_ICMP_SEND_ODD) {
        evenNotOdd = false;
    } else {
        Debug::out(LOG_DEBUG, "NetAdapter::icmpSend() called for unknown cmd[5]=%02x", cmd[5]);
        dataTrans->setStatus(E_PARAMETER);
        return;
    }

    DWORD destinIP  = Utils::getDword(cmd + 5);                     // get destination IP address
    int   icmpType  = cmd[9] >> 3;                                  // get ICMP type
    int   icmpCode  = cmd[9] & 0x07;                                // get ICMP code
    WORD  length    = Utils::getWord(cmd + 10);                     // get length of data to be sent
    
    bool res = dataTrans->recvData(dataBuffer, length);             // get data from Hans

    if(!res) {                                                      // failed to get data? internal error!
        Debug::out(LOG_DEBUG, "NetAdapter::icmpSend - failed to receive data...");
        dataTrans->setStatus(E_PARAMETER);
        return;
    }

    if(rawSock.isClosed()) {                                        // we don't have RAW socket yet? create it
        int rawFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
        
        if(rawFd == -1) {                                           // failed to create RAW socket? 
            Debug::out(LOG_DEBUG, "NetAdapter::icmpSend - failed to create RAW socket");
            dataTrans->setStatus(E_FNAVAIL);
            return;
        } else {
            Debug::out(LOG_DEBUG, "NetAdapter::icmpSend - RAW socket created");
        }

/*
        // IP_HDRINCL must be set on the socket so that the kernel does not attempt to automatically add a default ip header to the packet
        int optval = 0;
        setsockopt(rawFd, IPPROTO_IP, IP_HDRINCL, &optval, sizeof(int));
*/

        rawSock.fd  = rawFd;                                        // RAW socket created
        rawSockFd   = rawFd;
    }

    BYTE *pData = dataBuffer;                                       // pointer to where data starts
    if(!evenNotOdd) {                                               // if data is odd, it starts one byte further
        pData++;
    }

    WORD id         = Utils::getWord(pData);                        // get ID 
    WORD sequence   = Utils::getWord(pData + 2);                    // get sequence

    length = (length >= 4) ? (length - 4) : 0;                      // as 4 bytes have been already used, subtract 4 if possible, otherwise set to 0

    BYTE a,b,c,d;
    a = destinIP >> 24;
    b = destinIP >> 16;
    c = destinIP >>  8;
    d = destinIP      ;
    Debug::out(LOG_DEBUG, "NetAdapter::icmpSend -- will send ICMP data to %d.%d.%d.%d, ICMP type: %d, ICMP code: %d, ICMP id: %d, ICMP sequence: %d, data length: %d", a, b, c, d, icmpType, icmpCode, id, sequence, length);

    rawSockHeads.setIpHeader(localIp, destinIP, length);            // source IP, destination IP, data length
    rawSockHeads.setIcmpHeader(icmpType, icmpCode, id, sequence);   // ICMP ToS, ICMP code, ID, sequence
    
    rawSock.remote_adr.sin_family       = AF_INET;
    rawSock.remote_adr.sin_addr.s_addr  = htonl(destinIP);

    if(length > 0) {
        memcpy(rawSockHeads.data, pData + 4, length);               // copy the rest of data from received buffer to raw packet data
    }

    int ires = sendto(rawSock.fd, rawSockHeads.icmp, sizeof(struct icmphdr) + length, 0, (struct sockaddr *) &rawSock.remote_adr, sizeof(struct sockaddr));

    if(ires == -1) {                                                // on failure
        Debug::out(LOG_DEBUG, "NetAdapter::icmpSend -- sendto() failed, errno: %d", errno);
        dataTrans->setStatus(E_BADDNAME);
    } else {                                                        // on success
        dataTrans->setStatus(E_NORMAL);
    }
}
//----------------------------------------------
void NetAdapter::icmpGetDgrams(void)
{
    pthread_mutex_lock(&networkThreadMutex);
    
    if(icmpDataCount <= 0) {                                // nothing to read? fail, no data
        pthread_mutex_unlock(&networkThreadMutex);

        Debug::out(LOG_DEBUG, "NetAdapter::icmpGetDgrams -- no data, quit");
        dataTrans->setStatus(E_NODATA);
        return;
    }

    //--------------
    // find out how many dgrams we can fit into this transfer
    int sectorCount = cmd[5];                               // get sector count and byte count
    int byteCount   = sectorCount * 512;

    int gotCount    = 0;
    int gotBytes    = 2;

    int i; 
    for(i=0; i<MAX_STING_DGRAMS; i++) {                         // now count how many dgrams we can send before we run out of sectors
        if(!dgrams[i].isEmpty()) {
            if((gotBytes + dgrams[i].count + 2) > byteCount) {  // if adding this dgram would cause buffer overflow, quit
                break;
            }

            gotCount++;                                         // will fit into requested sectors, add it
            gotBytes += 2 + dgrams[i].count;                    // size of a datagram + WORD for its size
        }
    }

    Debug::out(LOG_DEBUG, "NetAdapter::icmpGetDgrams -- found %d ICMP Dgrams, they take %d bytes", gotCount, gotBytes);
    //------------
    // now will the data transporter with dgrams
    for(i=0; i<gotCount; i++) {
        int index = dgram_getNonEmpty();
        if(index == -1) {                               // no more dgrams? quit
            break;
        }

        Utils::storeWord(dgrams[index].data + 52, rawSockHeads.echoId);                     // fake this ECHO ID, because linux replaced the ECHO ID when sending ECHO packet
        
        dataTrans->addDataWord(dgrams[index].count);                                        // add size of this dgram
        dataTrans->addDataBfr((BYTE *) dgrams[index].data, dgrams[index].count, false);     // add the dgram
        
        Debug::out(LOG_DEBUG, "NetAdapter::icmpGetDgrams -- stored Dgram of length %d", dgrams[index].count);
        
        dgrams[index].clear();                                                              // empty it
    }   

    dataTrans->addDataWord(0);                          // terminate with a zero, that means no other DGRAM
    dataTrans->padDataToMul16();                        // pad to multiple of 16

    icmpDataCount = dgram_calcIcmpDataCount();
    Debug::out(LOG_DEBUG, "NetAdapter::icmpGetDgrams -- updated icmpDataCount to %d bytes", icmpDataCount);
    
    pthread_mutex_unlock(&networkThreadMutex);
    dataTrans->setStatus(E_NORMAL);
}
//----------------------------------------------
void NetAdapter::resolveStart(void)
{    
    bool res = dataTrans->recvData(dataBuffer, 512);    // get data from Hans

    if(!res) {                                          // failed to get data? internal error!
        Debug::out(LOG_DEBUG, "NetAdapter::resolveStart - failed to receive data...");
        dataTrans->setStatus(E_PARAMETER);
        return;
    }

    //-------------------
    // first handle weird names
    int len = strlen((char *)dataBuffer);
    int i;

    if(len <= 0) {              // address too short? fail
        Debug::out(LOG_DEBUG, "NetAdapter::resolveStart - host name too short");
        dataTrans->setStatus(E_CANTRESOLVE);
        return;
    }
    
    bool foundNormalChar = false;
    for(i=0; i<len; i++) {      // find any normal alpha numeric char
        
        if(isalnum((char) dataBuffer[i])) {
            foundNormalChar = true;
        }
    }
    
    if(!foundNormalChar) {      // no normal char? fail
        Debug::out(LOG_DEBUG, "NetAdapter::resolveStart - host name doesn't contain any normal character");
        dataTrans->setStatus(E_CANTRESOLVE);
        return;
    }
    
    //-------------------
    
    Debug::out(LOG_DEBUG, "NetAdapter::resolveStart - will resolve: %s", dataBuffer);
    
    int resolverHandle = resolver.addRequest((char *) dataBuffer);      // this is the input string param (the name)
    dataTrans->setStatus(resolverHandle);                               // return handle
}
//----------------------------------------------
void NetAdapter::resolveGetResp(void)
{
    int index = cmd[5];

    if(!resolver.slotIndexValid(index)) {                       // invalid index? E_PARAMETER
        dataTrans->setStatus(E_PARAMETER);
        return;
    }
    
    bool resolveDone = resolver.checkAndhandleSlot(index);      // check if resolved finished
    
    if(!resolveDone) {                                  // if the resolve command didn't finish yet
        Debug::out(LOG_DEBUG, "NetAdapter::resolveGetResp -- the resolved didn't finish yet for slot %d", index);
        dataTrans->setStatus(RES_DIDNT_FINISH_YET);     // return this special status
        return;
    }

    Tresolv *r = &resolver.requests[index];
    
    // if resolve did finish
    BYTE empty[256];
    memset(empty, 0, 256);

    int domLen = strlen(r->canonName);                                      // length of real domain name
    dataTrans->addDataBfr((BYTE *) r->canonName, domLen, false);            // store real domain name
    dataTrans->addDataBfr(empty, 256 - domLen, false);                      // add zeros to pad to 256 bytes

    dataTrans->addDataByte(r->count);                                       // data[256] = count of IP addreses resolved
    dataTrans->addDataByte(0);                                              // data[257] = just a dummy byte

    dataTrans->addDataBfr((BYTE *) r->data, 4 * r->count, true);            // now store all the resolved data, and pad to multiple of 16

    Debug::out(LOG_DEBUG, "NetAdapter::resolveGetResp -- returned %d IPs", r->count);
    
    resolver.clearSlot(index);                                              // clear the slot for next usage
    dataTrans->setStatus(E_NORMAL);
}
//----------------------------------------------
int NetAdapter::findEmptyConnectionSlot(void)
{
    int i;

    for(i=0; i<MAX_HANDLE; i++) {                   // try to find closed (empty) slot
        if(cons[i].isClosed()) {
            return i;
        }
    }

    return -1;                                      // no empty slot?
}
//----------------------------------------------

void NetAdapter::updateCons(void)
{
    int i;

    for(i=0; i<MAX_HANDLE; i++) {                   // update connection info                
        if(cons[i].isClosed()) {                    // if connection closed, skip it
            continue;
        }

        if(cons[i].activeNotPassive) {              // active? 
            updateCons_active(i);
        } else {                                    // passive?
            updateCons_passive(i);
        }      
    }
}

//----------------------------------------------
void NetAdapter::updateCons_passive(int i)
{
    if(cons[i].fd == -1 && cons[i].listenFd == -1) {                        // both sockets closed? quit
        return;
    }

    if(cons[i].fd == -1 && cons[i].listenFd != -1) {                        // got listening socket, but not normal socket? 
        struct sockaddr_storage remoteAddress;                              // this struct will receive the remote address if accept() succeeds
        socklen_t               addrSize;
        addrSize = sizeof(sockaddr);
        memset(&remoteAddress, 0, addrSize);
        
        int fd = accept(cons[i].listenFd, (struct sockaddr*) &remoteAddress, &addrSize);    // try to accept
        
        if(fd == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {         // failed, because nothing waiting? quit
            return;
        }

        Debug::out(LOG_DEBUG, "NetAdapter::updateCons_passive() -- connection %d - accept() succeeded, client connected", i);
        
        // ok, got the connection, store file descriptor and new state
        cons[i].fd      = fd;
        cons[i].status  = TESTABLISH;
        
        // also store the remote address that just connected to us
        if (remoteAddress.ss_family == AF_INET) {               // if it's IPv4
            struct sockaddr_in *s = (struct sockaddr_in *) &remoteAddress;
        
            cons[i].remote_adr.sin_addr.s_addr  = s->sin_addr.s_addr;
            cons[i].remote_adr.sin_port         = s->sin_port;

            Debug::out(LOG_DEBUG, "NetAdapter::updateCons_passive() -- connection %d - got IP & port of remote host", i);
        }
        
        return;
    }
    
    // ok, so we have both sockets? did the data socket HUP?
    if(didSocketHangUp(i)) {
        Debug::out(LOG_DEBUG, "NetAdapter::updateCons_passive() -- connection %d - poll returned POLLRDHUP, so closing", i);
    
        cons[i].status = TCLOSED;   // mark the state as TCLOSED
        cons[i].closeIt();
    }
}
//----------------------------------------------
void NetAdapter::updateCons_active(int i)
{
    int res;

    //---------
    // update how many bytes we can read from this sock
    cons[i].bytesInSocket = howManyWeCanReadFromFd(cons[i].fd);       // try to get how many bytes can be read
    Debug::out(LOG_DEBUG, "NetAdapter::updateCons_active() -- cons[%d].bytesInSocket: %d, status: cons[%d].status: %d", i, cons[i].bytesInSocket, i, cons[i].status);
    
    //---------
    // if it's not TCP connection, just pretend the state is 'connected' - it's only used for TCP connections, so it doesn't matter
    if(cons[i].type != TCP) {    
        Debug::out(LOG_DEBUG, "NetAdapter::updateCons_active() -- non-TCP connection %d -- now TESTABLISH", i);
        cons[i].status = TESTABLISH;
        return;
    }

    //---------
    // update connection status of TCP socket
    
    if(cons[i].status == TSYN_SENT) {               // if this is TCP connection and it's in the 'connecting' state
        res = connect(cons[i].fd, (struct sockaddr *) &cons[i].remote_adr, sizeof(cons[i].remote_adr));   // try to connect again

        if(res < 0) {                           // error occured on connect, check what it was
            switch(errno) {
                case EISCONN:
                case EALREADY:      cons[i].status = TESTABLISH;    
                                    Debug::out(LOG_DEBUG, "NetAdapter::updateCons_active() -- connection %d is now TESTABLISH", i);
                                    break;  // ok, we're connected!
                                    
                case EINPROGRESS:   cons[i].status = TSYN_SENT;     
                                    Debug::out(LOG_DEBUG, "NetAdapter::updateCons_active() -- connection %d is now TSYN_SENT", i);
                                    break;  // still trying to connect

                 // on failures
                case ETIMEDOUT:
                case ENETUNREACH:
                case ECONNREFUSED:  cons[i].status = TCLOSED;       
                                    cons[i].closeIt();
                                    Debug::out(LOG_DEBUG, "NetAdapter::updateCons_active() -- connection %d is now TCLOSED", i);
                                    break;
                                    
                default:
                                    Debug::out(LOG_DEBUG, "NetAdapter::updateCons_active() -- connect() returned error %d", errno);
                                    break;
            }
        } else if(res == 0) {                   // no error occured? 
            cons[i].status = TESTABLISH;
            Debug::out(LOG_DEBUG, "NetAdapter::updateCons_active() -- connection %d is now TESTABLISH - because no error", i);
        }
    } else {                                            // not connecting state? try to find out the state
        if(didSocketHangUp(i)) {
            Debug::out(LOG_DEBUG, "NetAdapter::updateCons_active() -- connection %d - poll returned POLLRDHUP, so closing", i);
    
            cons[i].status = TCLOSED;   // mark the state as TCLOSED
            cons[i].closeIt();
        }
    }
}

//----------------------------------------------
bool NetAdapter::didSocketHangUp(int i)
{
    if(cons[i].bytesInSocket != 0) {                // something to read? didn't HUP then!
        return false;
    }

    // nothing to read? see if it closed
    struct pollfd pfd;
    pfd.fd      = cons[i].fd;
    pfd.events  = POLLRDHUP | POLLERR | POLLHUP;
    pfd.revents = 0;

    int ires = poll(&pfd, 1, 0);                    // see if this connection is closed

    if(ires != 0) {
        if((pfd.revents & POLLRDHUP) != 0) {        // got POLLRDHUP event? it did HUP
            return true;
        }
    }

    return false;
}
//----------------------------------------------

int NetAdapter::howManyWeCanReadFromFd(int fd)
{
    int res, value;

    res = ioctl(fd, FIONREAD, &value);          // try to get how many bytes can be read
        
    if(res == -1) {                             // ioctl failed? 
        value = 0;
    }

    return value;
}
//----------------------------------------------
int NetAdapter::readFromSocket(TNetConnection *nc, int cnt)
{
    int countFromSocket = MIN(cnt, nc->bytesInSocket);      // how much can we read from socket?

    int res = recv(nc->fd, dataBuffer, countFromSocket, MSG_DONTWAIT);      // read the data

    Debug::out(LOG_DEBUG, "NetAdapter::readFromSocket -- cnt: %d, res: %d", cnt, res);
    
    if(res == -1) {                                         // failed to read? 
    
        return 0;
    }

    if(res > 0) {                                           // if some bytes have been read
        nc->prevLastByte    = dataBuffer[res - 1];          // store the last byte of buffer
        nc->gotPrevLastByte = true;

        dataTrans->addDataBfr(dataBuffer, res, false);      // put the data in data transporter
    }

    return res;                                             // return count
}
//----------------------------------------------
void NetAdapter::finishDataRead(TNetConnection *nc, int totalCnt, BYTE status)
{
    nc->lastReadCount = totalCnt;                               // store how many we have read

    dataTrans->padDataToMul16();                                // pad to multiple of 16
    dataTrans->setStatus(status);
}
//----------------------------------------------
void NetAdapter::logFunctionName(BYTE cmd) 
{
    switch(cmd){
        case NET_CMD_IDENTIFY:              

        // TCP functions
        case NET_CMD_TCP_OPEN:              Debug::out(LOG_DEBUG, "NET_CMD_TCP_OPEN");              break;
        case NET_CMD_TCP_CLOSE:             Debug::out(LOG_DEBUG, "NET_CMD_TCP_CLOSE");             break;
        case NET_CMD_TCP_SEND:              Debug::out(LOG_DEBUG, "NET_CMD_TCP_SEND");              break;
        case NET_CMD_TCP_WAIT_STATE:        Debug::out(LOG_DEBUG, "NET_CMD_TCP_WAIT_STATE");        break;
        case NET_CMD_TCP_ACK_WAIT:          Debug::out(LOG_DEBUG, "NET_CMD_TCP_ACK_WAIT");          break;
        case NET_CMD_TCP_INFO:              Debug::out(LOG_DEBUG, "NET_CMD_TCP_INFO");              break;

        // UDP FUNCTION
        case NET_CMD_UDP_OPEN:              Debug::out(LOG_DEBUG, "NET_CMD_UDP_OPEN");              break;
        case NET_CMD_UDP_CLOSE:             Debug::out(LOG_DEBUG, "NET_CMD_UDP_CLOSE");             break;
        case NET_CMD_UDP_SEND:              Debug::out(LOG_DEBUG, "NET_CMD_UDP_SEND");              break;

        // ICMP FUNCTIONS
        case NET_CMD_ICMP_SEND_EVEN:        Debug::out(LOG_DEBUG, "NET_CMD_ICMP_SEND_EVEN");        break;
        case NET_CMD_ICMP_SEND_ODD:         Debug::out(LOG_DEBUG, "NET_CMD_ICMP_SEND_ODD");         break;
        case NET_CMD_ICMP_HANDLER:          Debug::out(LOG_DEBUG, "NET_CMD_ICMP_HANDLER");          break;
        case NET_CMD_ICMP_DISCARD:          Debug::out(LOG_DEBUG, "NET_CMD_ICMP_DISCARD");          break;
        case NET_CMD_ICMP_GET_DGRAMS:       Debug::out(LOG_DEBUG, "NET_CMD_ICMP_GET_DGRAMS");       break;

        // CONNECTION MANAGER
        case NET_CMD_CNKICK:                Debug::out(LOG_DEBUG, "NET_CMD_CNKICK");                break;
        case NET_CMD_CNBYTE_COUNT:          Debug::out(LOG_DEBUG, "NET_CMD_CNBYTE_COUNT");          break;
        case NET_CMD_CNGET_CHAR:            Debug::out(LOG_DEBUG, "NET_CMD_CNGET_CHAR");            break;
        case NET_CMD_CNGET_NDB:             Debug::out(LOG_DEBUG, "NET_CMD_CNGET_NDB");             break;
        case NET_CMD_CNGET_BLOCK:           Debug::out(LOG_DEBUG, "NET_CMD_CNGET_BLOCK");           break;
        case NET_CMD_CNGETINFO:             Debug::out(LOG_DEBUG, "NET_CMD_CNGETINFO");             break;
        case NET_CMD_CNGETS:                Debug::out(LOG_DEBUG, "NET_CMD_CNGETS");                break;
        case NET_CMD_CN_UPDATE_INFO:        Debug::out(LOG_DEBUG, "NET_CMD_CN_UPDATE_INFO");        break;
        case NET_CMD_CN_READ_DATA:          Debug::out(LOG_DEBUG, "NET_CMD_CN_READ_DATA");          break;
        case NET_CMD_CN_GET_DATA_COUNT:     Debug::out(LOG_DEBUG, "NET_CMD_CN_GET_DATA_COUNT");     break;
        case NET_CMD_CN_LOCATE_DELIMITER:   Debug::out(LOG_DEBUG, "NET_CMD_CN_LOCATE_DELIMITER");   break;

        // MISC
        case NET_CMD_RESOLVE:               Debug::out(LOG_DEBUG, "NET_CMD_RESOLVE");               break;
        case NET_CMD_RESOLVE_GET_RESPONSE:  Debug::out(LOG_DEBUG, "NET_CMD_RESOLVE_GET_RESPONSE");  break;
        case NET_CMD_ON_PORT:               Debug::out(LOG_DEBUG, "NET_CMD_ON_PORT");               break;
        case NET_CMD_OFF_PORT:              Debug::out(LOG_DEBUG, "NET_CMD_OFF_PORT");              break;
        case NET_CMD_QUERY_PORT:            Debug::out(LOG_DEBUG, "NET_CMD_QUERY_PORT");            break;
        case NET_CMD_CNTRL_PORT:            Debug::out(LOG_DEBUG, "NET_CMD_CNTRL_PORT");            break;
        
        default:                            Debug::out(LOG_DEBUG, "NET_CMD - unknown command!");    break;
    }
}



