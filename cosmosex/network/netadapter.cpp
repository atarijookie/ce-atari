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

#include "../global.h"
#include "../debug.h"
#include "../utils.h"

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

void netReqAdd(TNetReq &tnr)
{
	pthread_mutex_lock(&networkThreadMutex);            // try to lock the mutex
	netReqQueue.push(tnr);                              // add this to queue
	pthread_mutex_unlock(&networkThreadMutex);          // unlock the mutex
}

void *networkThreadCode(void *ptr)
{
	Debug::out(LOG_INFO, "Network thread starting...");

	while(sigintReceived == 0) {
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
	
	Debug::out(LOG_INFO, "Network thread terminated.");
	return 0;
}

//--------------------------------------------------------

NetAdapter::NetAdapter(void)
{
    dataTrans = 0;
    dataBuffer  = new BYTE[NET_BUFFER_SIZE];

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
    } else if(bfr[5] == 1) {                        // wlan0 enabled? add its IP
        dataTrans->addDataBfr(bfr + 6, 4, false);
    } else {                                        // eth0 and wlan0 disabled? send zeros
        memset(bfr, 0, 4);
        dataTrans->addDataBfr(bfr, 4, false);
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
    WORD  tos           = Utils::getWord (dataBuffer + 6);
    WORD  buff_size     = Utils::getWord (dataBuffer + 8);

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
    cons[slot].bytesToReadInSocket  = 0;
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
        dataTrans->addDataDword(cons[i].bytesToReadInSocket + cons[i].bytesToReadInBuffer);
    }

    for(i=0; i<MAX_HANDLE; i++) {                           // store connection statuses
        dataTrans->addDataByte(cons[i].status);
    }

    dataTrans->addDataDword(0);                             // TODO: fill the real data to be read from ICMP sock

    dataTrans->setStatus(E_NORMAL);
}
//----------------------------------------------
void NetAdapter::conReadData(void)
{
    int   handle                = cmd[6];                       // get handle
    DWORD byteCountStRequested  = Utils::get24bits(cmd + 7);    // get how many bytes we want to read
    int   seekOffset            = (char) cmd[10];               // get seek offset (can be 0 or -1)

    if(handle < 0 || handle >= MAX_HANDLE) {                // handle out of range? fail
        dataTrans->setStatus(E_BADHANDLE);
        return;
    }

    TNetConnection *nc = &cons[handle];                     // will use this nc instead of longer cons[handle] in the next lines

    if(nc->isClosed()) {                                    // connection not open? fail
        dataTrans->setStatus(E_BADHANDLE);
        return;
    }

    int byteCountThatCanBeRead;
    nc->bytesToReadInSocket = howManyWeCanReadFromFd(nc->fd);                       // update how many bytes we have waiting in socket
    byteCountThatCanBeRead  = nc->bytesToReadInSocket + nc->bytesToReadInBuffer;    // byte count that can be read = bytes in socket + bytes in local buffer

    if(byteCountThatCanBeRead == 0) {                                               // nothing to read? return that we don't have enough data
        nc->lastReadCount = 0;
        dataTrans->setStatus(RW_PARTIAL_TRANSFER);
        return;
    }

    DWORD readCount = MIN(byteCountStRequested, byteCountThatCanBeRead);        // find out if you can read enough, or data would be missing

    // TODO: simplify the following code :)

    //-----------------
    // first use the data we already have in local buffer
    int dataCountFromLocalBuffer = MIN(readCount, nc->bytesToReadInBuffer);     // get the count that we will use from local buffer

    if(dataCountFromLocalBuffer > 0) {                                          // if there is something in local buffer

        if(seekOffset == -1 && nc->gotPrevLastByte) {                           // if we want to seek one byte back, and we do have that byte, do seek
            dataTrans->addDataByte(nc->prevLastByte);                           // add this byte as first
            nc->gotPrevLastByte = false;
        }

        dataTrans->addDataBfr(nc->rBfr, dataCountFromLocalBuffer + seekOffset, false);  // add data from local buffer

        int lastByteOffset = dataCountFromLocalBuffer + seekOffset - 1;
        if(lastByteOffset >= 0 && lastByteOffset < CON_BFR_SIZE) {
            nc->prevLastByte    = nc->rBfr[lastByteOffset];                     // store the last byte of buffer
            nc->gotPrevLastByte = true;
        }

        int i;
        int restOfBuffer = nc->bytesToReadInBuffer - dataCountFromLocalBuffer + seekOffset; // count of bytes there are after the bytes we just transfered
        for(i=0; i<restOfBuffer; i++) {
            nc->rBfr[i] = nc->rBfr[i + dataCountFromLocalBuffer];               // move the not used data to the start of the local buffer
        }        

        nc->bytesToReadInBuffer = restOfBuffer;                                 // store the new count of bytes we still have in local buffer

        if(dataCountFromLocalBuffer == byteCountStRequested) {                  // if this was all what ST wanted, fine, let's quit
            nc->lastReadCount = dataCountFromLocalBuffer;

            dataTrans->padDataToMul16();
            dataTrans->setStatus(RW_ALL_TRANSFERED);
            return;
        }

        readCount = readCount - dataCountFromLocalBuffer;                       // what we want to read now is the rest - the whole count without what we already read from local buffer
    }

    //-----------------
    // now try to read the rest from socket
    if(seekOffset == -1 && nc->gotPrevLastByte) {           // if we want to seek one byte back, and we do have that byte, do seek
        dataTrans->addDataByte(nc->prevLastByte);           // add this byte as first
        nc->gotPrevLastByte = false;
        readCount--;
    }

    int res = read(nc->fd, dataBuffer, readCount);          // read the data

    if(res == -1) {                                         // failed to read? 
        nc->lastReadCount = dataCountFromLocalBuffer;
        dataTrans->setStatus(RW_PARTIAL_TRANSFER);
        return;
    }

    if(res > 0) {                                           // if some bytes have been read
        nc->prevLastByte    = dataBuffer[res - 1];          // store the last byte of buffer
        nc->gotPrevLastByte = true;
    }

    // read successful?
    dataTrans->addDataBfr(dataBuffer, res, true);                       // put the data in data transporter
    nc->lastReadCount = dataCountFromLocalBuffer + res - seekOffset;    // store the last data count

    if(nc->lastReadCount < byteCountStRequested) {          // didn't read as many as wished? partial transfer
        dataTrans->setStatus(RW_PARTIAL_TRANSFER);
    } else {                                                // did read everythen as wanted - all was read
        dataTrans->setStatus(RW_ALL_TRANSFERED);
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
    if(nc->bytesToReadInBuffer < CON_BFR_SIZE) {                // ok, we got some space, let's try to fill it
        int emptyCnt = CON_BFR_SIZE - nc->bytesToReadInBuffer;  // we have this many bytes free in buffer
        int canRead  = howManyWeCanReadFromFd(nc->fd);          // find out how many bytes we can read

        int readCount = MIN(canRead, emptyCnt);                 // get the smaller number out of these two

        int res = read(nc->fd, nc->rBfr + nc->bytesToReadInBuffer, readCount);  // read the data to the end of the previous data

        if(res > 0) {                                           // read succeeded? Increment count in local read buffer
            nc->bytesToReadInBuffer += res;
        }
    }
    //-----------------
    // now try to find the delimiter
    int i;
    bool found = false;
    for(i=0; i<nc->bytesToReadInBuffer; i++) {          // go through the buffer and find delimiter
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
    bool evenNotOdd;

    if(cmd[5] == NET_CMD_ICMP_SEND_EVEN) {
        evenNotOdd = true;
    } else if(cmd[5] == NET_CMD_ICMP_SEND_ODD) {
        evenNotOdd = false;
    } else {
        dataTrans->setStatus(E_PARAMETER);
        return;
    }

    DWORD destinIP  = Utils::getDword(cmd + 6);         // get destination IP address
    int icmpType    = cmd[10] >> 3;                     // get ICMP type
    int icmpCode    = cmd[10] & 0x07;                   // get ICMP code
    WORD length     = Utils::getWord(cmd + 11);         // get length of data to be sent

    



    dataTrans->setStatus(E_NORMAL);
}
//----------------------------------------------
void NetAdapter::icmpGetDgrams(void)
{


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
    dataTrans->addDataBfr(empty, 256, false);                               // first 256 empty for now - should be real domain name (later)
    
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
    int value;

    for(i=0; i<MAX_HANDLE; i++) {                   // update connection info                
        if(cons[i].isClosed()) {                    // if connection closed, skip it
            continue;
        }

        //---------
        // update how many bytes we can read from this sock
        cons[i].bytesToReadInSocket = howManyWeCanReadFromFd(cons[i].fd);       // try to get how many bytes can be read

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

                    case ETIMEDOUT:
                    case ENETUNREACH:
                    case ECONNREFUSED:  cons[i].status = TCLOSED;       break;  // on failures
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



