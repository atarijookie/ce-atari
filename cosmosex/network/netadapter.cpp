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

struct Tresolv {
    volatile BYTE           done;
             int            count;
             BYTE           data[128];
             std::string    h_name;
} resolv;

//--------------------------------------------------------

pthread_mutex_t networkThreadMutex = PTHREAD_MUTEX_INITIALIZER;
std::queue<TNetReq> netReqQueue;

DWORD localIp;
int   rawSockFd = -1;
bool  tryIcmpRecv(BYTE *bfr);

#define RECV_BFR_SIZE   (64 * 1024)

#define MAX_STING_DGRAMS    32
TStringDgram dgrams[MAX_STING_DGRAMS];
volatile DWORD icmpDataCount;

int dgram_getEmpty(void) {
    int i; 
    for(i=0; i<MAX_STING_DGRAMS; i++) {
        if(dgrams[i].isEmpty()) {
            return i;
        }
    }

    return -1;
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
	Debug::out(LOG_INFO, "Network thread starting...");
    BYTE *recvBfr = new BYTE[RECV_BFR_SIZE];            // 64 kB receive buffer    

	while(sigintReceived == 0) {
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

        // resolve host name to IP addresses
        if(tnr.type == NETREQ_TYPE_RESOLVE) {
            struct hostent *he;
            he = gethostbyname((char *) tnr.strParam.c_str());      // try to resolve

            if (he == NULL) {
                resolv.count    = 0;
                resolv.done     = true;
                continue;
            }

            resolv.h_name = (char *) he->h_name;                    // store official name

            struct in_addr **addr_list;
            addr_list = (struct in_addr **) he->h_addr_list;        // get pointer to IP addresses

            int i, cnt = 0;     
            DWORD *pIps = (DWORD *) resolv.data;

            for(i=0; addr_list[i] != NULL; i++) {                   // now walk through the IP address list
                in_addr ia = *addr_list[i];
                pIps[cnt] = ia.s_addr;                              // store the current IP
                cnt++;

                if(cnt >= (128 / 4)) {                              // if we have the maximum possible IPs we can store, quit
                    break;
                }
            }

            resolv.count    = cnt;                                  // store count and we're done
            resolv.done     = true;
            continue;
        }

    }

    delete []recvBfr;
	
	Debug::out(LOG_INFO, "Network thread terminated.");
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
        return false;
    }

    // res now contains length of ICMP packet (header + data)

    //-----------------------
    // parse response to the right structs
    pthread_mutex_lock(&networkThreadMutex);

    int i = dgram_getEmpty();       // now find space for the datagram
    if(i == -1) {                   // no space? fail, but return that we were able to receive data
        pthread_mutex_unlock(&networkThreadMutex);
        return true;
    }

    TStringDgram *d = &dgrams[i];
    d->clear();

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

    loadSettings();
}

NetAdapter::~NetAdapter()
{
    delete []dataBuffer;
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
    Debug::out(LOG_INFO, "NetAdapter::loadSettings");

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
void NetAdapter::conOpen(void)
{
    bool tcpNotUdp;
    int  ires;

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
    DWORD remoteHost    = Utils::getDword(dataBuffer);
    WORD  remotePort    = Utils::getWord (dataBuffer + 4);

/*
    // following 2 params can be received, but are not used for now
    WORD  tos           = Utils::getWord (dataBuffer + 6);
    WORD  buff_size     = Utils::getWord (dataBuffer + 8);
*/

    int fd;
    if(tcpNotUdp) {     // TCP connection
        fd = socket(AF_INET, SOCK_STREAM, 0);
    } else {            // UDP connection
        fd = socket(AF_INET, SOCK_DGRAM, 0);
    }

    if(fd < 0) {        // failed? quit
        Debug::out(LOG_DEBUG, "NetAdapter::conOpen - socket() failed");
        dataTrans->setStatus(E_CONNECTFAIL);
        return;
    }

    // fill the remote host info
    struct sockaddr_in serv_addr;
    memset(&serv_addr, '0', sizeof(serv_addr)); 
    serv_addr.sin_family        = AF_INET;
    serv_addr.sin_addr.s_addr   = remoteHost;
    serv_addr.sin_port          = htons(remotePort); 

    if(tcpNotUdp) {                                             // for TCP sockets
        int flags = fcntl(fd, F_GETFL, 0);                      // get flags
        ires = fcntl(fd, F_SETFL, flags | O_NONBLOCK);          // set it as non-blocking

        if(ires == -1) {
            Debug::out(LOG_DEBUG, "NetAdapter::conOpen - setting O_NONBLOCK failed, but continuing");
        }
    }

    // for TCP - try to connect, for UPD - set the default address where to send data
    ires = connect(fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));  

    if(ires < 0 && errno != EINPROGRESS) {      // if connect failed, and it's not EINPROGRESS (because it's O_NONBLOCK)
        close(fd);

        Debug::out(LOG_DEBUG, "NetAdapter::conOpen - connect() failed");
        dataTrans->setStatus(E_CONNECTFAIL);
        return;
    }

    int conStatus = TESTABLISH;                 // for UDP and blocking TCP, this is 'we have connection'
    if(ires < 0 && errno == EINPROGRESS) {      // if it's a O_NONBLOCK socket connecting, the state is connecting
        conStatus = TSYN_SENT;                  // for non-blocking TCP, this is 'we're trying to connect'
    }
    
    cons[slot].initVars();                      // init vars

    // store the info
    cons[slot].fd                   = fd;
    cons[slot].hostAdr              = serv_addr;
    cons[slot].type                 = tcpNotUdp ? TCP : UDP;
    cons[slot].bytesInSocket  = 0;
    cons[slot].status               = conStatus;

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
        dataTrans->setStatus(E_PARAMETER);
        return;
    }

    if(cons[handle].isClosed()) {                       // handle already closed? fail
        dataTrans->setStatus(E_BADHANDLE);
        return;
    }

    cons[handle].closeIt();                             // handle good, close it

    dataTrans->setStatus(E_NORMAL);
}
//----------------------------------------------
void NetAdapter::conSend(void)
{
    int  cmdType    = cmd[5];                           // command type: NET_CMD_TCP_SEND or NET_CMD_UDP_SEND
    int  handle     = cmd[6];                           // connection handle
    int  length     = Utils::getWord(cmd + 7);          // get data length
    bool isOdd      = cmd[9];                           // if the data was send from odd address, this will be non-zero...
    BYTE oddByte    = cmd[10];                          // ...and this will contain the 0th byte

    if(handle < 0 || handle >= MAX_HANDLE) {            // handle out of range? fail
        dataTrans->setStatus(E_BADHANDLE);
        return;
    }

    if(cons[handle].isClosed()) {                       // connection not open? fail
        dataTrans->setStatus(E_BADHANDLE);
        return;
    }

    bool good = false;                                  // check if trying to do right type of send over right type of connection (TCP over TCP, UDP over UDP)
    if( (cmdType == NET_CMD_TCP_SEND && cons[handle].type == TCP) || 
        (cmdType == NET_CMD_UDP_SEND && cons[handle].type == UDP)) {
        good = true;
    }

    if(!good) {                                         // send type vs. connection type mismatch? fail
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

    int ires = write(cons[handle].fd, pData, length);   // try to send the data

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

    // fill the buffer
    for(i=0; i<MAX_HANDLE; i++) {                           // store how many bytes we can read from connections
        dataTrans->addDataDword(cons[i].bytesInSocket + cons[i].bytesInBuffer);
    }

    for(i=0; i<MAX_HANDLE; i++) {                           // store connection statuses
        dataTrans->addDataByte(cons[i].status);
    }

    pthread_mutex_lock(&networkThreadMutex);
    dataTrans->addDataDword(icmpDataCount);                 // fill the data to be read from ICMP sock
    pthread_mutex_unlock(&networkThreadMutex);

    dataTrans->setStatus(E_NORMAL);
}
//----------------------------------------------
void NetAdapter::conReadData(void)
{
    int   handle                = cmd[6];                       // get handle
    DWORD byteCountStRequested  = Utils::get24bits(cmd + 7);    // get how many bytes we want to read
    int   seekOffset            = (char) cmd[10];               // get seek offset (can be 0 or -1)

    if(handle < 0 || handle >= MAX_HANDLE) {                    // handle out of range? fail
        dataTrans->setStatus(E_BADHANDLE);
        return;
    }

    TNetConnection *nc = &cons[handle];                         // will use this nc instead of longer cons[handle] in the next lines

    if(nc->isClosed()) {                                        // connection not open? fail
        dataTrans->setStatus(E_BADHANDLE);
        return;
    }

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
            finishDataRead(nc, totalCnt, RW_ALL_TRANSFERED);    // update variables, set status
            return;
        }
    }

    //--------------
    // now find out if we can read anything
    nc->bytesInSocket           = howManyWeCanReadFromFd(nc->fd);           // update how many bytes we have waiting in socket
    int byteCountThatCanBeRead  = nc->bytesInSocket + nc->bytesInBuffer;    // byte count that can be read = bytes in socket + bytes in local buffer

    if(byteCountThatCanBeRead == 0) {                                       // nothing to read? return that we don't have enough data
        finishDataRead(nc, totalCnt, RW_PARTIAL_TRANSFER);                  // update variables, set status
        return;
    }

    //-----------------
    // now use the data we already have in local buffer
    int cntLoc = readFromLocalBuffer(nc, toRead);

    toRead      -= cntLoc;
    totalCnt    += cntLoc;

    if(toRead == 0) {                                               // we don't need to read more? quit with success
        finishDataRead(nc, totalCnt, RW_ALL_TRANSFERED);            // update variables, set status
        return;
    }

    //-----------------
    // and try to read the rest from socket
    int cntSck = readFromSocket(nc, toRead);

    toRead      -= cntSck;
    totalCnt    += cntSck;

    if(toRead == 0) {                                               // we don't need to read more? quit with success
        finishDataRead(nc, totalCnt, RW_ALL_TRANSFERED);            // transfered all that was wanted
    } else {
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
    int  handle = cmd[6];                               // get connection handle
    BYTE delim  = cmd[7];                               // get string delimiter
    
    if(handle < 0 || handle >= MAX_HANDLE) {            // handle out of range? fail
        dataTrans->setStatus(E_BADHANDLE);
        return;
    }

    TNetConnection *nc = &cons[handle];

    if(nc->isClosed()) {                                // connection not open? fail
        dataTrans->setStatus(E_BADHANDLE);
        return;
    }

    //-----------------
    // try to fill the local read buffer, if there's space
    if(nc->bytesInBuffer < CON_BFR_SIZE) {                // ok, we got some space, let's try to fill it
        int emptyCnt = CON_BFR_SIZE - nc->bytesInBuffer;  // we have this many bytes free in buffer
        int canRead  = howManyWeCanReadFromFd(nc->fd);          // find out how many bytes we can read

        int readCount = MIN(canRead, emptyCnt);                 // get the smaller number out of these two

        int res = read(nc->fd, nc->rBfr + nc->bytesInBuffer, readCount);  // read the data to the end of the previous data

        if(res > 0) {                                           // read succeeded? Increment count in local read buffer
            nc->bytesInBuffer += res;
        }
    }
    //-----------------
    // now try to find the delimiter
    int i;
    bool found = false;
    for(i=0; i<nc->bytesInBuffer; i++) {          // go through the buffer and find delimiter
        if(nc->rBfr[i] == delim) {                      // delimiter found?
            found = true;
            break;
        }
    }

    if(found) {                                         // if found, store position
        dataTrans->addDataDword(i);
    } else {                                            // not found? return DELIMITER_NOT_FOUND
        dataTrans->addDataDword(DELIMITER_NOT_FOUND);
    }
    dataTrans->padDataToMul16();                        // make sure the data size is multiple of 16

    dataTrans->setStatus(E_NORMAL);                     // this operation was OK
}
//----------------------------------------------
void NetAdapter::icmpSend(void)
{
    // is this needed? sysctl -w net.ipv4.ping_group_range="0 0" 

    bool evenNotOdd;

    if(cmd[5] == NET_CMD_ICMP_SEND_EVEN) {
        evenNotOdd = true;
    } else if(cmd[5] == NET_CMD_ICMP_SEND_ODD) {
        evenNotOdd = false;
    } else {
        dataTrans->setStatus(E_PARAMETER);
        return;
    }

    DWORD destinIP  = Utils::getDword(cmd + 6);                     // get destination IP address
    int   icmpType  = cmd[10] >> 3;                                 // get ICMP type
    int   icmpCode  = cmd[10] & 0x07;                               // get ICMP code
    WORD  length    = Utils::getWord(cmd + 11);                     // get length of data to be sent

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

    rawSockHeads.setIpHeader(localIp, destinIP, length);            // source IP, destination IP, data length
    rawSockHeads.setIcmpHeader(icmpType, icmpCode, id, sequence);   // ICMP ToS, ICMP code, ID, sequence
    
    rawSock.hostAdr.sin_family      = AF_INET;
    rawSock.hostAdr.sin_addr.s_addr = htonl(destinIP);

    if(length > 0) {
        memcpy(rawSockHeads.data, pData + 4, length);               // copy the rest of data from received buffer to raw packet data
    }

    int ires = sendto(rawSock.fd, rawSockHeads.icmp, sizeof(struct icmphdr) + length, 0, (struct sockaddr *) &rawSock.hostAdr, sizeof(struct sockaddr));

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

    //------------
    // now will the data transporter with dgrams
    for(i=0; i<gotCount; i++) {
        int index = dgram_getNonEmpty();
        if(index == -1) {                               // no more dgrams? quit
            break;
        }

        dataTrans->addDataWord(dgrams[index].count);                                        // add size of this dgram
        dataTrans->addDataBfr((BYTE *) dgrams[index].data, dgrams[index].count, false);     // add the dgram
        dgrams[index].clear();                                                              // empty it
    }   

    dataTrans->addDataWord(0);                          // terminate with a zero, that means no other DGRAM
    dataTrans->padDataToMul16();                        // pad to multiple of 16

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

    Debug::out(LOG_DEBUG, "NetAdapter::resolveStart - will resolve: %s", dataBuffer);
    
    // try to resolve the name asynchronously
    resolv.done     = false;                    // mark that this hasn't finished yet

    TNetReq tnr;
    tnr.type        = NETREQ_TYPE_RESOLVE;      // request type
    tnr.strParam    = (char *) dataBuffer;      // this is the input string param (the name)
    netReqAdd(tnr);

    dataTrans->setStatus(E_NORMAL);
}
//----------------------------------------------
void NetAdapter::resolveGetResp(void)
{
    if(!resolv.done) {                                  // if the resolve command didn't finish yet
        dataTrans->setStatus(RES_DIDNT_FINISH_YET);     // return this special status
        return;
    }

    // if resolve did finish
    BYTE empty[256];
    memset(empty, 0, 256);

    int domLen = resolv.h_name.length();                                    // length of real domain name
    dataTrans->addDataBfr((BYTE *) resolv.h_name.c_str(), domLen, false);   // store real domain name
    dataTrans->addDataBfr(empty, 256 - domLen, false);                      // add zeros to pad to 256 bytes

    dataTrans->addDataByte(resolv.count);                                   // data[256] = count of IP addreses resolved
    dataTrans->addDataByte(0);                                              // data[257] = just a dummy byte

    dataTrans->addDataBfr((BYTE *) resolv.data, 4 * resolv.count, true);    // now store all the resolved data, and pad to multiple of 16

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
    int i, res;

    for(i=0; i<MAX_HANDLE; i++) {                   // update connection info                
        if(cons[i].isClosed()) {                    // if connection closed, skip it
            continue;
        }

        //---------
        // update how many bytes we can read from this sock
        cons[i].bytesInSocket = howManyWeCanReadFromFd(cons[i].fd);       // try to get how many bytes can be read

        //---------
        // if it's not TCP connection, just pretend the state is 'connected' - it's only used for TCP connections, so it doesn't matter
        if(cons[i].type != TCP) {    
            cons[i].status = TESTABLISH;
            continue;
        }

        //---------
        // update connection status
        
        if(cons[i].status == TSYN_SENT) {               // if this is TCP connection and it's in the 'connecting' state
            res = connect(cons[i].fd, (struct sockaddr *) &cons[i].hostAdr, sizeof(cons[i].hostAdr));   // try to connect again

            if(res < 0) {                               // error occured on connect, check what it was
                switch(errno) {
                    case EALREADY:      cons[i].status = TESTABLISH;    break;  // ok, we're connected!
                    case EINPROGRESS:   cons[i].status = TSYN_SENT;     break;  // still trying to connect

                     // on failures
                    case ETIMEDOUT:
                    case ENETUNREACH:
                    case ECONNREFUSED:  cons[i].status = TCLOSED;       
                                        cons[i].closeIt();
                                        break; 
                }
            }
        } else {                                        // if it's not TCP socket in connecting state, try to find out the state
            int error, len;
            len = sizeof(int);
            res = getsockopt(cons[i].fd, SOL_SOCKET, SO_ERROR, &error, (socklen_t *) &len);

            if(res == -1) {                             // getsockopt failed? don't update status
                continue;
            }

            if(error == 0) {                            // no error? connection good
                cons[i].status = TESTABLISH;
            } else {                                    // some error? close connection, this will also set the status to closed
                cons[i].closeIt();
            }
        }
    }
}
//----------------------------------------------
int NetAdapter::howManyWeCanReadFromFd(int fd)
{
    int res, value;

    res = ioctl(fd, FIONREAD, &value);          // try to get how many bytes can be read
        
    if(res == -1) {                             // ioctl failed? 
        return 0;
    }

    return value;
}
//----------------------------------------------
int NetAdapter::readFromLocalBuffer(TNetConnection *nc, int cnt)
{
    int countFromBuffer = MIN(cnt, nc->bytesInBuffer);          // get the count that we will use from local buffer

    if(countFromBuffer == 0) {                                  // nothing to read? quit
        return 0;
    }

    dataTrans->addDataBfr(nc->rBfr, countFromBuffer, false);    // add data from local buffer

    nc->prevLastByte    = nc->rBfr[countFromBuffer - 1];        // store the last byte of buffer
    nc->gotPrevLastByte = true;

    int i;
    int restOfBuffer = nc->bytesInBuffer - countFromBuffer;     // count of bytes there are after the bytes we just transfered

    for(i=0; i<restOfBuffer; i++) {
        nc->rBfr[i] = nc->rBfr[i + countFromBuffer];            // move the not used data to the start of the local buffer
    }        

    nc->bytesInBuffer = restOfBuffer;                           // store the new count of bytes we still have in local buffer
    return countFromBuffer;                                     // return that we've read this many bytes
}
//----------------------------------------------
int NetAdapter::readFromSocket(TNetConnection *nc, int cnt)
{
    int countFromSocket = MIN(cnt, nc->bytesInSocket);      // how much can we read from socket?

    int res = read(nc->fd, dataBuffer, countFromSocket);    // read the data

    if(res == -1) {                                         // failed to read? 
        return 0;
    }

    if(res > 0) {                                           // if some bytes have been read
        nc->prevLastByte    = dataBuffer[res - 1];          // store the last byte of buffer
        nc->gotPrevLastByte = true;

        dataTrans->addDataBfr(dataBuffer, res, false);          // put the data in data transporter
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




