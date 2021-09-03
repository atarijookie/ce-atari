// vim: shiftwidth=4 tabstop=4 softtabstop=4 expandtab
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
#include "icmpwrapper.h"

extern   uint32_t localIp;

IcmpWrapper::IcmpWrapper(void)
{
    recvBfr = new uint8_t[RECV_BFR_SIZE];
    rawSock = new TNetConnection();
}

IcmpWrapper::~IcmpWrapper(void)
{
    delete []recvBfr;
    delete rawSock;
}

void IcmpWrapper::closeAndClean(void)
{
    rawSock->closeIt();                         // close raw / icmp socket
    
    for(int i=0; i<MAX_STING_DGRAMS; i++) {     // clear received icmp dgrams
        dgrams[i].clear();
    }
}

void IcmpWrapper::clearOld(void) 
{
    uint32_t now = Utils::getCurrentMs();
    
    for(int i=0; i<MAX_STING_DGRAMS; i++) {     // find non-empty slot, and if it's old, clear it
        if(dgrams[i].isEmpty()) {               // found empty? skip it
            continue;
        }
        
        uint32_t diff = now - dgrams[i].time;      // calculate how old is this dgram
        if(diff < 10000) {                      // dgram is younger than 10 seconds? skip it
            continue;
        }

        dgrams[i].clear();                      // it's too old, clear it
        Debug::out(LOG_DEBUG, "IcmpWrapper::clearOld() - dgram #%d was too old and it was cleared", i);
    }
}

int IcmpWrapper::getEmptyIndex(void) {
    int i; 
    uint32_t oldestTime    = 0xffffffff;
    int   oldestIndex   = 0;
    
    for(i=0; i<MAX_STING_DGRAMS; i++) {     // try to find empty slot
        if(dgrams[i].isEmpty()) {           // found empty? return it
            return i;
        }

        if(dgrams[i].time < oldestTime) {   // found older item than we had found before? store index
            oldestTime  = dgrams[i].time;
            oldestIndex = i;
        }
    }

    // no empty slot found, clear and return the oldest - to avoid filling up the dgrams array (at the cost of loosing oldest items)
    Debug::out(LOG_DEBUG, "IcmpWrapper::getEmptyIndex() - no empty slot, returning oldest slot - #%d", oldestIndex);
    
    dgrams[oldestIndex].clear();
    return oldestIndex;
}

int IcmpWrapper::getNonEmptyIndex(void) {
    int i; 
    for(i=0; i<MAX_STING_DGRAMS; i++) {
        if(!dgrams[i].isEmpty()) {
            return i;
        }
    }

    return -1;
}

uint32_t IcmpWrapper::calcDataByteCountTotal(void) 
{
    uint32_t sum = 0;
    int i; 

    for(i=0; i<MAX_STING_DGRAMS; i++) {     // go through received DGRAMs
        if(!dgrams[i].isEmpty()) {          // not empty?
            sum += dgrams[i].count;         // add size of this DGRAM
        }
    }

    return sum;
}

int IcmpWrapper::calcHowManyDatagramsFitIntoBuffer(int bufferSizeBytes)
{
    int gotCount    = 0;
    int gotBytes    = 2;

    int i; 
    for(i=0; i<MAX_STING_DGRAMS; i++) {                                 // now count how many dgrams we can send before we run out of sectors
        if(!dgrams[i].isEmpty()) {
            if((gotBytes + dgrams[i].count + 2) > bufferSizeBytes) {    // if adding this dgram would cause buffer overflow, quit
                break;
            }

            gotCount++;                                                 // will fit into requested sectors, add it
            gotBytes += 2 + dgrams[i].count;                            // size of a datagram + uint16_t for its size
        }
    }

    Debug::out(LOG_DEBUG, "IcmpWrapper::calcHowManyDatagramsFitIntoBuffer() -- found %d ICMP Dgrams, they take %d bytes", gotCount, gotBytes);
    return gotCount;
}

void IcmpWrapper::receiveAll(void)
{
    clearOld();                 // clear old dgrams that are probably stuck in the queue 

    while(1) {                  // receive all available ICMP data 
        bool r = receive();     // this will fail if no data available
        if(!r) {                // if receiving failed, quit; otherwise do another receiving!
            break;
        }
    }
}

bool IcmpWrapper::receive(void)
{
    if(rawSock->fd == -1) {                                   // ICMP socket closed? quit, no data
        return false;
    }

    //-----------------------
    // receive the data
    // recvfrom will receive only one ICMP packet, even if there are more than 1 packets waiting in socket
    struct sockaddr_in src_addr;
    socklen_t addrlen = sizeof(struct sockaddr);

    ssize_t res = recvfrom(rawSock->fd, recvBfr, RECV_BFR_SIZE, MSG_DONTWAIT, (struct sockaddr *) &src_addr, &addrlen);

    if(res == -1) {                 // if recvfrom failed, no data
        if(errno != EAGAIN && errno != EWOULDBLOCK) {
            Debug::out(LOG_ERROR, "IcmpWrapper::receive() - recvfrom() failed, errno: %d", errno);
        } 

        return false;
    } else if (res == 0) {
        Debug::out(LOG_ERROR, "IcmpWrapper::receive() - recvfrom() returned 0");
        return false;
    }

    // res now contains length of ICMP packet (header + data)
    Debug::out(LOG_DEBUG, "IcmpWrapper::receive() %d bytes from %s", res, inet_ntoa(src_addr.sin_addr));

    //-----------------------
    // parse response to the right structs
    int i = getEmptyIndex();        // now find space for the datagram
    if(i == -1) {                   // no space? fail, but return that we were able to receive data
        Debug::out(LOG_DEBUG, "IcmpWrapper::receive() - dgram_getEmpty() returned -1");
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

    Utils::storeDword(d->data + 12, ntohl(src_addr.sin_addr.s_addr));     // data[12 .. 15] - source IP
    Utils::storeDword(d->data + 16, localIp);                           // data[16 .. 19] - destination IP 

    uint16_t checksum = TRawSocks::checksum((uint16_t *) d->data, 20);
    Utils::storeWord(d->data + 10, checksum);                           // calculate chekcsum, store to data[10 .. 11]
    //-------------
    // fill IP_DGRAM header
    Utils::storeWord(d->data + 30, res);                                // data[30 .. 31] - pkt_length - length of IP packet data block (res)
    Utils::storeDword(d->data + 32, 128);                               // data[32 .. 35] - timeout - timeout of packet life

    //-------------
    // now append ICMP packet

    int rest = MIN(STING_DGRAM_MAXSIZE - 48 - 2, res);                  // we can store only (STING_DGRAM_MAXSIZE - 48 - 2) = 462 bytes of ICMP packets to fit 512 B
    memcpy(d->data + 48, recvBfr, rest);                                // copy whole ICMP packet beyond the IP_DGRAM structure

    Utils::storeWord(d->data + 52, rawSockHeads.echoId);                // fake this ECHO ID, because linux replaced the ECHO ID when sending ECHO packet
    //--------------
    // epilogue - update stuff, unlock mutex, success!
    d->count = 48 + rest;                                               // update how many bytes this gram contains all together

    icmpDataCount = calcDataByteCountTotal();                           // update icmpDataCount

    Debug::out(LOG_DEBUG, "IcmpWrapper::receive() - icmpDataCount is now %d bytes", icmpDataCount);
    return true;
}

uint8_t IcmpWrapper::send(uint32_t destinIP, int icmpType, int icmpCode, uint16_t length, uint8_t *data)
{
    if(rawSock->isClosed()) {                                       // we don't have RAW socket yet? create it
        // grant right to open (AF_INET, SOCK_DGRAM, IPPROTO_ICMP) sockets
        // also possible via sysctl
        // system("sysctl -w net.ipv4.ping_group_range=\"0 0\" > /dev/null");
        FILE * f = fopen("/proc/sys/net/ipv4/ping_group_range", "w");
        if(f) {
            fprintf(f, "0 0\n");
            fclose(f);
        }
        int rawFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
        
        if(rawFd == -1) {                                           // failed to create RAW socket? 
            Debug::out(LOG_DEBUG, "IcmpWrapper::send() - failed to create RAW socket : %s", strerror(errno));
            return E_FNAVAIL;
        }
        Debug::out(LOG_DEBUG, "IcmpWrapper::send() - RAW socket created fd #%d", rawFd);

/*
        // IP_HDRINCL must be set on the socket so that the kernel does not attempt to automatically add a default ip header to the packet
        int optval = 0;
        setsockopt(rawFd, IPPROTO_IP, IP_HDRINCL, &optval, sizeof(int));
*/

        rawSock->fd = rawFd;                                        // RAW socket created
    }

    uint16_t id         = Utils::getWord(data);                         // get ID 
    uint16_t sequence   = Utils::getWord(data + 2);                     // get sequence

    length = (length >= 4) ? (length - 4) : 0;                      // as 4 bytes have been already used, subtract 4 if possible, otherwise set to 0

    uint8_t a,b,c,d;
    a = destinIP >> 24;
    b = destinIP >> 16;
    c = destinIP >>  8;
    d = destinIP      ;
    Debug::out(LOG_DEBUG, "IcmpWrapper::send() -- will send ICMP data to %d.%d.%d.%d, ICMP type: %d, ICMP code: %d, ICMP id: %d, ICMP sequence: %d, data length: %d", a, b, c, d, icmpType, icmpCode, id, sequence, length);

    rawSockHeads.setIpHeader(localIp, destinIP, length);            // source IP, destination IP, data length
    rawSockHeads.setIcmpHeader(icmpType, icmpCode, id, sequence);   // ICMP ToS, ICMP code, ID, sequence
    
    rawSock->remote_adr.sin_family       = AF_INET;
    rawSock->remote_adr.sin_addr.s_addr  = htonl(destinIP);

    if(length > 0) {
        memcpy(rawSockHeads.data, data + 4, length);                // copy the rest of data from received buffer to raw packet data
    }

    int ires = sendto(rawSock->fd, rawSockHeads.icmp, sizeof(struct icmphdr) + length, 0, (struct sockaddr *) &rawSock->remote_adr, sizeof(struct sockaddr));

    if(ires == -1) {                                                // on failure
        Debug::out(LOG_DEBUG, "IcmpWrapper::send() -- sendto() failed, errno: %d", errno);
        return E_BADDNAME;
    } 

    return E_NORMAL;
}


