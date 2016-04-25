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

void netReqAdd(TNetReq &tnr)
{
	pthread_mutex_lock(&networkThreadMutex);            // try to lock the mutex
	netReqQueue.push(tnr);                              // add this to queue
	pthread_mutex_unlock(&networkThreadMutex);          // unlock the mutex
}

void *networkThreadCode(void *ptr)
{
	Debug::out(LOG_DEBUG, "Network thread starting...");

    // The following line is needed to allow root to create raw sockets for ICMP echo... 
    // The problem with this is it allows only ICMP echo to be done.
    system("sysctl -w net.ipv4.ping_group_range=\"0 0\" > /dev/null");  

/*
	while(sigintReceived == 0) {
        pthread_mutex_lock(&networkThreadMutex);

        pthread_mutex_unlock(&networkThreadMutex);
        Utils::sleepMs(100); 						    // wait 100 ms and try again

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
*/

	Debug::out(LOG_DEBUG, "Network thread terminated.");
	return 0;
}

//--------------------------------------------------------

NetAdapter::NetAdapter(void)
{
    dataTrans       = 0;
    dataBuffer      = new BYTE[NET_BUFFER_SIZE];
    localIp         = 0;

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

        case NET_CMD_CNGET_CHAR:            conGetCharBuffer(); break;
        case NET_CMD_CNGET_NDB:             conGetNdb();        break;
        case NET_CMD_CNGET_BLOCK:           conGetBlock();      break;
        case NET_CMD_CNGETS:                conGetString();     break;

        case NET_CMD_CNBYTE_COUNT:          break;                      // currently not used on host
        case NET_CMD_CNGETINFO:             break;                      // currently not used on host
        case NET_CMD_CN_UPDATE_INFO:        conUpdateInfo();    break;

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
    dataTrans->addDataBfr("CosmosEx network module", 24, false);   // add 24 bytes which are the identification string
    
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
    for(i=0; i<NET_HANDLES_COUNT; i++) {            // close normal sockets
        cons[i].closeIt();
        cons[i].cleanIt();
    }

    icmpWrapper.closeAndClean();    
    
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
        
    // type of service (tos) is not used for now, buff_size is used for faking packet size in CNget_NDB()
    WORD  tos           = Utils::getWord (dataBuffer + 6);
    WORD  buff_size     = Utils::getWord (dataBuffer + 8);

    // local port, useful mainly for pasive (listening) connections
    WORD  localPort     = Utils::getWord (dataBuffer + 14);

    Debug::out(LOG_DEBUG, "NetAdapter::conOpen() -- remoteHost: %d.%d.%d.%d, remotePort: %d, buff_size: %d, localPort: %d", (BYTE) (remoteHost >> 24), (BYTE) (remoteHost >> 16), (BYTE) (remoteHost >> 8), (BYTE) (remoteHost), remotePort, buff_size, localPort);
    Debug::out(LOG_DEBUG, "NetAdapter::conOpen() -- will %s", (connectNotListen ? "connect to host" : "listen for connection"));

    if(connectNotListen) {      // connect to remote host (active connection)
        conOpen_connect(slot, tcpNotUdp, localPort, remoteHost, remotePort, tos, buff_size);
    } else {                    // listen for connection  (passive connection)
        conOpen_listen (slot, tcpNotUdp, localPort, remoteHost, remotePort, tos, buff_size);
    }
}

void NetAdapter::conOpen_listen(int slot, bool tcpNotUdp, WORD localPort, DWORD remoteHost, WORD remotePort, WORD tos, WORD buff_size)
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
    
    //------------------
    int optval = 1;
    ires = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));
    
    if(ires == -1) {
        Debug::out(LOG_DEBUG, "NetAdapter::conOpen_listen - setsockopt(SO_REUSEADDR) failed with errno: %d", errno);
    }
    //------------------
    
    // fill the local address info
    struct sockaddr_in local_addr;
    memset(&local_addr, '0', sizeof(local_addr)); 
    local_addr.sin_family        = AF_INET;
    local_addr.sin_addr.s_addr   = htonl(INADDR_ANY);
    local_addr.sin_port          = htons(localPort);                    // if localPort is 0, it will bind to random free port
    
    ires = bind(fd, (struct sockaddr*) &local_addr, sizeof(local_addr));    // bind IP & port to this socket
    
    if(ires == -1) {                                                    // if bind failed, quit
        close(fd);
        
        Debug::out(LOG_DEBUG, "NetAdapter::conOpen_listen - bind() failed with error: %d", errno);
        dataTrans->setStatus(E_CONNECTFAIL);
        return;
    }
    
    listen(fd, 1);                                                      // mark this socket as listening, with queue length of 1

    if(localPort == 0) {                                                // should listen on 1st free port?
        localPort = getLocalPort(fd);   
        Debug::out(LOG_DEBUG, "NetAdapter::conOpen_listen - now listening on first free port %d (hex: 0x%04x, dec: %d, %d)", localPort, localPort, localPort >> 8, localPort & 0xff);
    } else {
        Debug::out(LOG_DEBUG, "NetAdapter::conOpen_listen - now listening on specified port %d (hex: 0x%04x, dec: %d, %d)",  localPort, localPort, localPort >> 8, localPort & 0xff);
    }

    TNetConnection *nc = &cons[slot];
    nc->initVars();                                 // init vars

    // store the info
    nc->activeNotPassive = false;                   // it's passive (listening) socket
    nc->listenFd         = fd;
    nc->localPort        = localPort;               // store local port - either the specified one, or the first free which was assigned
    nc->type             = tcpNotUdp ? TCP : UDP;
    nc->bytesInSocket    = 0;
    nc->status           = TLISTEN;
    nc->buff_size        = buff_size;

    // return the handle
    BYTE connectionHandle = network_slotToHandle(slot);
    Debug::out(LOG_DEBUG, "NetAdapter::conOpen_listen() - returning %d as handle for slot %d", (int) connectionHandle, slot);
    dataTrans->setStatus(connectionHandle);
}

WORD NetAdapter::getLocalPort(int sockFd)
{
    WORD localPort = 0;

    struct sockaddr_in real_addr;
    memset(&real_addr, '0', sizeof(sockaddr_in)); 

    socklen_t len = (socklen_t) sizeof(real_addr);
    int ires = getsockname(sockFd, (struct sockaddr*) &real_addr, &len);    // try to find out real local port number that was open / used
    
    if(ires == -1) {
        Debug::out(LOG_DEBUG, "NetAdapter::getLocalPort - getsockname() failed with error: %d", errno);
        return 0;
    }
    
    localPort = ntohs(real_addr.sin_port);                          // get the real local port

    Debug::out(LOG_DEBUG, "NetAdapter::getLocalPort - getsockname() returned local port: %d", localPort);
    return localPort;
}

void NetAdapter::conOpen_connect(int slot, bool tcpNotUdp, WORD localPort, DWORD remoteHost, WORD remotePort, WORD tos, WORD buff_size)
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

    if(tcpNotUdp) {
        setKeepAliveOptions(fd);                // configure keep alive on TCP socket
    }

    int conStatus = TESTABLISH;                 // for UDP and blocking TCP, this is 'we have connection'
    if(ires < 0 && errno == EINPROGRESS) {      // if it's a O_NONBLOCK socket connecting, the state is connecting
        conStatus = TSYN_SENT;                  // for non-blocking TCP, this is 'we're trying to connect'
        Debug::out(LOG_DEBUG, "NetAdapter::conOpen_connect() - non-blocking TCP is connecting, going to TSYN_SENT status");
    }
    
    if(localPort == 0) {                        // if local port not specified, find out current local port
        localPort = getLocalPort(fd);
    }
    
    TNetConnection *nc = &cons[slot];
    nc->initVars();                             // init vars

    // store the info
    nc->activeNotPassive = true;                // it's active (outgoing) socket
    nc->fd               = fd;
    nc->remote_adr       = serv_addr;
    nc->localPort        = localPort;               // store local port - either the specified one, or the first free which was assigned
    nc->type             = tcpNotUdp ? TCP : UDP;
    nc->bytesInSocket    = 0;
    nc->status           = conStatus;
    nc->buff_size         = buff_size;

    nc->readWrapper.init(fd, nc->type, buff_size);

    // return the handle
    BYTE connectionHandle = network_slotToHandle(slot);
    Debug::out(LOG_DEBUG, "NetAdapter::conOpen_connect() - returning %d as handle for slot %d", (int) connectionHandle, slot);
    dataTrans->setStatus(connectionHandle);
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

    int handle  = Utils::getWord(dataBuffer);           // retrieve handle

    if(!network_handleIsValid(handle)) {                // handle out of range? fail
        Debug::out(LOG_DEBUG, "NetAdapter::conClose() -- bad handle: %d", handle);
        dataTrans->setStatus(E_PARAMETER);
        return;
    }
    int slot = network_handleToSlot(handle);

    if(cons[slot].isClosed()) {                         // handle already closed? fail
        Debug::out(LOG_DEBUG, "NetAdapter::conClose() -- slot %d is already closed, pretending that it was closed :)", slot);
        dataTrans->setStatus(E_NORMAL);
        return;
    }

    Debug::out(LOG_DEBUG, "NetAdapter::conClose() -- closing connection with handle %d on slot %d", handle, slot);
    cons[slot].closeIt();                             // handle good, close it

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

    if(!network_handleIsValid(handle)) {                // handle out of range? fail
        Debug::out(LOG_DEBUG, "NetAdapter::conSend() -- bad handle: %d", handle);
        dataTrans->setStatus(E_PARAMETER);
        return;
    }
    int slot = network_handleToSlot(handle);

    if(cons[slot].isClosed()) {                         // connection not open? fail
        Debug::out(LOG_DEBUG, "NetAdapter::conSend() -- connection %d is closed", slot);
        dataTrans->setStatus(E_BADHANDLE);
        return;
    }

    bool good = false;                                  // check if trying to do right type of send over right type of connection (TCP over TCP, UDP over UDP)
    if( (cmdType == NET_CMD_TCP_SEND && cons[slot].type == TCP) || 
        (cmdType == NET_CMD_UDP_SEND && cons[slot].type == UDP)) {
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

    Debug::out(LOG_DEBUG, "NetAdapter::conSend() -- sending %d bytes through connection %d (received %d from ST, isOdd: %d)", length, slot, lenRoundUp, isOdd);
//  Debug::outBfr(pData, length);
    
    int ires = write(cons[slot].fd, dataBuffer, length);  // try to send the data

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
    for(i=0; i<NET_HANDLES_COUNT; i++) {                    // store how many bytes we can read from connections
        TNetConnection *ci = &cons[i];
        dataTrans->addDataDword(ci->bytesInSocket);
        
        if(ci->bytesInSocket > 0) {                         // something to read?
            Debug::out(LOG_DEBUG, "NetAdapter::conUpdateInfo - connection %d has %d bytes waiting to be read", i, ci->bytesInSocket);
            found++;
        }
        
        if(ci->status != TCLOSED) {                         // not closed?
            Debug::out(LOG_DEBUG, "NetAdapter::conUpdateInfo [%d] - status: %d, localPort: %d, remoteHost: %08x, remotePort: %d, bytesInSocket: %d", i, ci->status, ci->localPort, ntohl(ci->remote_adr.sin_addr.s_addr), ntohs(ci->remote_adr.sin_port), ci->bytesInSocket);
        }
    }

    for(i=0; i<NET_HANDLES_COUNT; i++) {                    // store connection statuses
        dataTrans->addDataByte(cons[i].status);
    }

    for(i=0; i<NET_HANDLES_COUNT; i++) {                    // store local ports (LPort)
        dataTrans->addDataWord(cons[i].localPort);
    }

    for(i=0; i<NET_HANDLES_COUNT; i++) {                    // store remote addresses (RHost)
        dataTrans->addDataDword(ntohl(cons[i].remote_adr.sin_addr.s_addr));
    }

    for(i=0; i<NET_HANDLES_COUNT; i++) {                    // store remote ports (RPort)
        dataTrans->addDataWord(ntohs(cons[i].remote_adr.sin_port));
    }

    icmpWrapper.receiveAll();
    DWORD imcpCnt = icmpWrapper.calcDataByteCountTotal();
    dataTrans->addDataDword(imcpCnt);                       // fill the data to be read from ICMP sock

    Debug::out(LOG_DEBUG, "NetAdapter::conUpdateInfo - imcpCnt: %d", imcpCnt);

    dataTrans->padDataToMul16();
    dataTrans->setStatus(E_NORMAL);
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

    BYTE *pData = dataBuffer;                                       // pointer to where data starts
    if(!evenNotOdd) {                                               // if data is odd, it starts one byte further
        pData++;
    }

    BYTE result;
    result = icmpWrapper.send(destinIP, icmpType, icmpCode, length, pData);
    dataTrans->setStatus(result);
}
//----------------------------------------------
void NetAdapter::icmpGetDgrams(void)
{
    pthread_mutex_lock(&networkThreadMutex);

    icmpWrapper.receiveAll();
    DWORD icmpByteCount = icmpWrapper.calcDataByteCountTotal();    

    if(icmpByteCount <= 0) {                                        // nothing to read? quit, no data
        pthread_mutex_unlock(&networkThreadMutex);

        Debug::out(LOG_DEBUG, "NetAdapter::icmpGetDgrams -- no data, quit");
        dataTrans->setStatus(E_NODATA);
        return;
    }

    //--------------
    // find out how many dgrams we can fit into this transfer
    int bufferSizeSectors   = cmd[5];                               // get sector count and byte count
    int bufferSizeBytes     = bufferSizeSectors * 512;

    int howManyDgramsWillFit = icmpWrapper.calcHowManyDatagramsFitIntoBuffer(bufferSizeBytes);

    Debug::out(LOG_DEBUG, "NetAdapter::icmpGetDgrams -- I can fit %d datagrams into the buffer of %d bytes", howManyDgramsWillFit, bufferSizeBytes);

    //------------
    // now fill the data transporter with dgrams
    for(int i=0; i<howManyDgramsWillFit; i++) {
        int index = icmpWrapper.getNonEmptyIndex();
        if(index == -1) {                                           // no more dgrams? quit
            break;
        }

        TStingDgram *d = &icmpWrapper.dgrams[index];

        dataTrans->addDataWord(d->count);                           // add size of this dgram
        dataTrans->addDataBfr(d->data, d->count, false);   // add the dgram
        
        Debug::out(LOG_DEBUG, "NetAdapter::icmpGetDgrams -- stored Dgram of length %d", d->count);
        
        d->clear();                                                 // empty it
    }   

    dataTrans->addDataWord(0);                                      // terminate with a zero, that means no other DGRAM
    dataTrans->padDataToMul16();                                    // pad to multiple of 16
    dataTrans->setStatus(E_NORMAL);

    pthread_mutex_unlock(&networkThreadMutex);
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
    dataTrans->addDataBfr(r->canonName, domLen, false);            // store real domain name
    dataTrans->addDataBfr(empty, 256 - domLen, false);                      // add zeros to pad to 256 bytes

    dataTrans->addDataByte(r->count);                                       // data[256] = count of IP addreses resolved
    dataTrans->addDataByte(0);                                              // data[257] = just a dummy byte

    dataTrans->addDataBfr(r->data, 4 * r->count, true);            // now store all the resolved data, and pad to multiple of 16

    Debug::out(LOG_DEBUG, "NetAdapter::resolveGetResp -- returned %d IPs", r->count);
    
    resolver.clearSlot(index);                                              // clear the slot for next usage
    dataTrans->setStatus(E_NORMAL);
}
//----------------------------------------------
int NetAdapter::findEmptyConnectionSlot(void)
{
    int i;

    for(i=0; i<NET_HANDLES_COUNT; i++) {            // try to find closed (empty) slot
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

    for(i=0; i<NET_HANDLES_COUNT; i++) {            // update connection info                
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

    TNetConnection *nc = &cons[i];

    if(nc->fd == -1 && nc->listenFd != -1) {                                // got listening socket, but not normal socket? 
        struct sockaddr_storage remoteAddress;                              // this struct will receive the remote address if accept() succeeds
        socklen_t               addrSize;
        addrSize = sizeof(sockaddr);
        memset(&remoteAddress, 0, addrSize);
        
        int fd = accept(nc->listenFd, (struct sockaddr*) &remoteAddress, &addrSize);    // try to accept
        
        if(fd == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {         // failed, because nothing waiting? quit
            return;
        }

        Debug::out(LOG_DEBUG, "NetAdapter::updateCons_passive() -- connection %d - accept() succeeded, client connected", i);

        //----------     
        setKeepAliveOptions(fd);                                // configure keep alive

        // ok, got the connection, store file descriptor and new state
        nc->fd      = fd;
        nc->status  = TESTABLISH;
        
        nc->readWrapper.init(fd, nc->type, nc->buff_size);

        // also store the remote address that just connected to us
        if (remoteAddress.ss_family == AF_INET) {               // if it's IPv4
            struct sockaddr_in *s = (struct sockaddr_in *) &remoteAddress;
        
            nc->remote_adr.sin_addr.s_addr  = s->sin_addr.s_addr;
            nc->remote_adr.sin_port         = s->sin_port;

            Debug::out(LOG_DEBUG, "NetAdapter::updateCons_passive() -- connection %d - got IP & port of remote host", i);
        }
        
        return;
    }
    
    if(nc->fd != -1) {
        nc->bytesInSocket = nc->readWrapper.getCount();         // try to get how many bytes can be read
        Debug::out(LOG_DEBUG, "NetAdapter::updateCons_passive(%d) -- bytesInSocket: %d, status: %d", i, nc->bytesInSocket, nc->status);
    }
    
    // ok, so we have both sockets? did the data socket HUP?
    if(didSocketHangUp(i)) {
        Debug::out(LOG_DEBUG, "NetAdapter::updateCons_passive() -- connection %d - poll returned POLLRDHUP, so closing", i);
    
        nc->status = TCLOSED;       // mark the state as TCLOSED
        nc->closeIt();
    }
}
//----------------------------------------------
void NetAdapter::updateCons_active(int i)
{
    int res;

    TNetConnection *nc = &cons[i];

    //---------
    // update how many bytes we can read from this sock
    nc->bytesInSocket = nc->readWrapper.getCount();         // try to get how many bytes can be read
    Debug::out(LOG_DEBUG, "NetAdapter::updateCons_active(%d) -- bytesInSocket: %d, status: %d", i, nc->bytesInSocket, nc->status);
    
    //---------
    // if it's not TCP connection, just pretend the state is 'connected' - it's only used for TCP connections, so it doesn't matter
    if(nc->type != TCP) {    
        Debug::out(LOG_DEBUG, "NetAdapter::updateCons_active() -- non-TCP connection %d -- now TESTABLISH", i);
        nc->status = TESTABLISH;
        return;
    }

    //---------
    // update connection status of TCP socket
    
    if(nc->status == TSYN_SENT) {               // if this is TCP connection and it's in the 'connecting' state
        res = connect(nc->fd, (struct sockaddr *) &nc->remote_adr, sizeof(nc->remote_adr));   // try to connect again

        if(res < 0) {                           // error occured on connect, check what it was
            switch(errno) {
                case EISCONN:
                case EALREADY:      nc->status = TESTABLISH;    
                                    Debug::out(LOG_DEBUG, "NetAdapter::updateCons_active() -- connection %d is now TESTABLISH", i);
                                    break;  // ok, we're connected!
                                    
                case EINPROGRESS:   nc->status = TSYN_SENT;     
                                    Debug::out(LOG_DEBUG, "NetAdapter::updateCons_active() -- connection %d is now TSYN_SENT", i);
                                    break;  // still trying to connect

                 // on failures
                case ETIMEDOUT:
                case ENETUNREACH:
                case ECONNREFUSED:  nc->status = TCLOSED;       
                                    nc->closeIt();
                                    Debug::out(LOG_DEBUG, "NetAdapter::updateCons_active() -- connection %d is now TCLOSED", i);
                                    break;
                                    
                default:
                                    Debug::out(LOG_DEBUG, "NetAdapter::updateCons_active() -- connect() returned error %d", errno);
                                    break;
            }
        } else if(res == 0) {                   // no error occured? 
            nc->status = TESTABLISH;
            Debug::out(LOG_DEBUG, "NetAdapter::updateCons_active() -- connection %d is now TESTABLISH - because no error", i);
        }
    } else {                                            // not connecting state? try to find out the state
        if(didSocketHangUp(i)) {
            Debug::out(LOG_DEBUG, "NetAdapter::updateCons_active() -- connection %d - poll returned POLLRDHUP, so closing", i);
    
            nc->status = TCLOSED;   // mark the state as TCLOSED
            nc->closeIt();
        }
    }
}

void NetAdapter::setKeepAliveOptions(int fd)
{
    // turning on keep alive on TCP socket
    int keepAliveEnabled = 1;
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &keepAliveEnabled, sizeof(int));

    // seting params of keepalive
    int keepcnt     = 5;    // The maximum number of keepalive probes TCP should send before dropping the connection. 
    int keepidle    = 30;   // The time (in seconds) the connection needs to remain idle before TCP starts sending keepalive probes
    int keepintvl   = 60;   // The time (in seconds) between individual keepalive probes.

    setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT,    &keepcnt,   sizeof(int));
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE,   &keepidle,  sizeof(int));
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL,  &keepintvl, sizeof(int));
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
void NetAdapter::conGetCharBuffer(void)
{
    // long command
    // cmd[4] = NET_CMD_CNGET_CHAR
    int handle = cmd[5];                                // get handle

    if(!network_handleIsValid(handle)) {                // handle out of range? fail
        Debug::out(LOG_DEBUG, "NetAdapter::conGetCharBuffer() -- bad handle: %d", handle);
        dataTrans->setStatus(E_PARAMETER);
        return;
    }
    int slot = network_handleToSlot(handle);

    TNetConnection *nc = &cons[slot];

    int charsUsed = cmd[9];                             // cmd[10] - how many chars were used by calling CNget_char() - we need to remove them first
    if(charsUsed > 0) {                                 // some chars were used, remove them
        Debug::out(LOG_DEBUG, "NetAdapter::conGetCharBuffer() -- CNget_char() used %d bytes, removing them from socket", charsUsed);
        nc->readWrapper.removeBlock(charsUsed);
    }

    int gotBytes = nc->readWrapper.peekBlock(dataBuffer, 250);  // peek data from socket - less than 255, because 'charsUsed' is sent as byte (for removing the last used chars from deque)
    dataTrans->addDataByte(gotBytes);                   // first store how many bytes we will return (this will be 250 bytes or less)

    Debug::out(LOG_DEBUG, "NetAdapter::conGetCharBuffer() -- will return buffer of %d bytes", gotBytes);

    dataTrans->addDataBfr(dataBuffer, gotBytes, true);  // add that char buffer to data buffer, pad to mul 16
    dataTrans->setStatus(E_NORMAL);
}

void NetAdapter::conGetNdb(void)
{
    // long command
    // cmd[4]           = NET_CMD_CNGET_NDB
    int handle          = cmd[5];                       // get handle
    int getNdbNotSize   = cmd[6];                       // If zero, returns just size. If non-zero, return data.

    if(!network_handleIsValid(handle)) {                // handle out of range? fail
        Debug::out(LOG_DEBUG, "NetAdapter::conGetNdb() -- bad handle: %d", handle);
        dataTrans->setStatus(E_PARAMETER);
        return;
    }
    int slot = network_handleToSlot(handle);

    TNetConnection *nc = &cons[slot];

    int charsUsed = cmd[9];                             // cmd[10] - how many chars were used by calling CNget_char() - we need to remove them first
    if(charsUsed > 0) {                                 // some chars were used, remove them
        Debug::out(LOG_DEBUG, "NetAdapter::conGetNdb() -- CNget_char() used %d bytes, removing them from socket", charsUsed);
        nc->readWrapper.removeBlock(charsUsed);
    }

    if(getNdbNotSize) {
        int ndbSize = nc->readWrapper.getNdb(dataBuffer);   // read data from socket (wrapper)

        Debug::out(LOG_DEBUG, "NetAdapter::conGetNdb() -- returning NDB data, size %d bytes", ndbSize);

        dataTrans->addDataBfr(dataBuffer, ndbSize, true);   // add data buffer, pad to mul 16
        dataTrans->setStatus(E_NORMAL);
    } else {
        int nextSizeBytes   = nc->readWrapper.getNextNdbSize();
        int nextSizeSectors = (nextSizeBytes / 512) + ((nextSizeBytes % 512) == 0 ? 0 : 1);

        Debug::out(LOG_DEBUG, "NetAdapter::conGetNextNdbSize() -- returning NDB size, nextSizeBytes: %d, nextSizeSectors: %d", nextSizeBytes, nextSizeSectors);

        dataTrans->addDataDword(nextSizeBytes);             // send byte count
        dataTrans->addDataByte(nextSizeSectors);            // send sector count

        dataTrans->padDataToMul16();                        // pad to multiple of 16
        dataTrans->setStatus(E_NORMAL);
    }
}

//----------------------------------------------

void NetAdapter::conGetBlock(void)
{
    // Long command
    // cmd[4] = NET_CMD_CNGET_BLOCK

    int handle          = cmd[5];                       // cmd[6]      - handle
    int wantedLength    = Utils::getWord(cmd + 6);      // cmd[7 .. 8] - block length

    Debug::out(LOG_DEBUG, "NetAdapter::conGetBlock() -- from handle %d get %d bytes", handle, wantedLength);

    if(!network_handleIsValid(handle)) {                // handle out of range? fail
        Debug::out(LOG_DEBUG, "NetAdapter::conGetBlock() -- bad handle: %d", handle);
        dataTrans->setStatus(E_PARAMETER);
        return;
    }
    int slot = network_handleToSlot(handle);

    TNetConnection *nc  = &cons[slot];

    int charsUsed = cmd[9];                             // cmd[10]     - how many chars were used by calling CNget_char() - we need to remove them first
    if(charsUsed > 0) {                                 // some chars were used, remove them
        Debug::out(LOG_DEBUG, "NetAdapter::conGetBlock() -- CNget_char() used %d bytes, removing them from socket", charsUsed);
        nc->readWrapper.removeBlock(charsUsed);
    }

    int gotBytes = nc->readWrapper.getCount();          // find out how many bytes we got for reading

    if(gotBytes < wantedLength) {                       // not enough data? fail
        Debug::out(LOG_DEBUG, "NetAdapter::conGetBlock() -- not enough data - wanted %d, got only %d", wantedLength, gotBytes);
        dataTrans->setStatus(E_NODATA);
        return;
    }

    Debug::out(LOG_DEBUG, "NetAdapter::conGetBlock() -- OK", wantedLength, gotBytes);

    nc->readWrapper.peekBlock(dataBuffer, wantedLength);    // peek   data from socket
    nc->readWrapper.removeBlock(wantedLength);              // remove data from socket

    dataTrans->addDataBfr(dataBuffer, wantedLength, true);  // add data buffer, pad to mul 16
    dataTrans->setStatus(E_NORMAL);    
}

void NetAdapter::conGetString(void)
{
    // Long command
    // cmd[4] = NET_CMD_CNGETS

    int handle      = cmd[5];                       // cmd[6]      - handle    
    int maxLength   = Utils::getWord(cmd + 6);      // cmd[7 .. 8] - max length
    BYTE delim      = cmd[8];                       // cmd[9]      - string delimiter / terminator

    if(!network_handleIsValid(handle)) {                // handle out of range? fail
        Debug::out(LOG_DEBUG, "NetAdapter::conGetString() -- bad handle: %d", handle);
        dataTrans->setStatus(E_PARAMETER);
        return;
    }
    int slot = network_handleToSlot(handle);

    TNetConnection *nc  = &cons[slot];

    int charsUsed = cmd[9];                         // cmd[10]     - how many chars were used by calling CNget_char() - we need to remove them first
    if(charsUsed > 0) {                             // some chars were used, remove them
        Debug::out(LOG_DEBUG, "NetAdapter::conGetString() -- CNget_char() used %d bytes, removing them from socket", charsUsed);
        nc->readWrapper.removeBlock(charsUsed);
    }

    int streamLength    = nc->readWrapper.getCount();       // find out how many bytes we have

    nc->readWrapper.peekBlock(dataBuffer, streamLength);    // peek the whole stream
    
    int i, foundIndex = -1;
    for(i=0; i<streamLength; i++) {                         // now search the whole stream for the delimiter
        if(dataBuffer[i] == delim) {
            foundIndex = i;
            break;
        }
    }

    if(foundIndex == -1) {                          // if delimiter not found, E_NODATA
        Debug::out(LOG_DEBUG, "NetAdapter::conGetString() -- delimiter not found");
        dataTrans->setStatus(E_NODATA);
        return;
    }

    if(foundIndex >= maxLength) {                   // if input buffer is not big enough to get the stream, E_BIGBUF
        Debug::out(LOG_DEBUG, "NetAdapter::conGetString() -- delimiter found, but input buffer too small");
        dataTrans->setStatus(E_BIGBUF);
        return;
    }

    dataBuffer[foundIndex] = 0;                                 // remove the delimiter
    dataTrans->addDataBfr(dataBuffer, foundIndex + 1, true);    // add data buffer (including terminating zero), pad to mul 16

    Debug::out(LOG_DEBUG, "NetAdapter::conGetString() -- delimiter found at index %d, returning string '%s'", foundIndex, dataBuffer);

    nc->readWrapper.removeBlock(foundIndex + 1);            // remove the string from stream (and remove the delimiter)
    dataTrans->setStatus(E_NORMAL);    
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



