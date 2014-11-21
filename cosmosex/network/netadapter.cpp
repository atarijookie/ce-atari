#include <string.h>
#include <stdio.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h> 

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
			Utils::sleepMs(20);  						// wait 20 ms and try again
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

    if(connect(fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {        // try to connect
        close(fd);

        Debug::out(LOG_DEBUG, "NetAdapter::conOpen - connect() failed");
        dataTrans->setStatus(E_CONNECTFAIL);
        return;
    } 
    
    // store the info
    cons[slot].fd           = fd;
    cons[slot].type         = tcpNotUdp ? TCP : UDP;
    cons[slot].bytesToRead  = 0;
    cons[slot].status       = TSYN_SENT;

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

    // TODO: update connections status and bytes to read

    // prepare the pointers
    DWORD *pBytesToRead     = (DWORD *) (dataBuffer      );
    BYTE  *pConnStatus      = (BYTE  *) (dataBuffer + 128);
    DWORD *pBytesToReadIcmp = (DWORD *) (dataBuffer + 160);

    // fill the buffer
    for(i=0; i<MAX_HANDLE; i++) {                   // store how many bytes we can read from connections, what's the connection status
        pBytesToRead[i] = cons[i].bytesToRead;
        pConnStatus[i]  = cons[i].status;
    }

    *pBytesToReadIcmp = 0;                          // TODO: fill the real data to be read from ICMP sock

    // pass it to ACSI data transporter
    dataTrans->addDataBfr(dataBuffer, 164, true);   // send all the data to ST, pad to be multiple of 16
    dataTrans->setStatus(E_NORMAL);
}
//----------------------------------------------
void NetAdapter::conReadData(void)
{

    dataTrans->setStatus(E_NORMAL);
}
//----------------------------------------------
void NetAdapter::conGetDataCount(void)
{

    dataTrans->setStatus(E_NORMAL);
}
//----------------------------------------------
void NetAdapter::conLocateDelim(void)
{

    dataTrans->setStatus(E_NORMAL);
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

