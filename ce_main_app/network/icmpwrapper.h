// vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab
#ifndef _ICMP_H_
#define _ICMP_H_

#include <stdlib.h>
#include <string.h>
#include <netinet/ip.h>
#include <linux/icmp.h>

#include "../utils.h"
#include "../global.h"
#include "../debug.h"

class TNetConnection;
#define RECV_BFR_SIZE   ( 64 * 1024)
#define CON_BFR_SIZE    (100 * 1024)

//-------------------------------------
#define STING_DGRAM_MAXSIZE     512

class TStingDgram {
public:
    TStingDgram() {
        data = new uint8_t[STING_DGRAM_MAXSIZE];
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

    uint8_t  *data;
    uint16_t  count;
    uint32_t time;
};

//-------------------------------------

class TRawSocks{
public:
    TRawSocks() {
        packet      = (char*) new uint8_t[ sizeof(struct iphdr) + sizeof(struct icmphdr) + CON_BFR_SIZE];

        ip          = (struct iphdr*)    packet;                                                    // pointer to IP   header (start of packet)
        icmp        = (struct icmphdr*) (packet + sizeof(struct iphdr));                            // pointer to ICMP header (that is just beyond IP header)
        data        = (uint8_t *)          (packet + sizeof(struct iphdr) + sizeof(struct icmphdr));   // pointer to ICMP data   (located beyond ICMP header)
    }

    ~TRawSocks() {
        delete []packet;
    }

    void setIpHeader(uint32_t src_addr, uint32_t dst_addr, uint16_t dataLength) {
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
        ip->check       = checksum((uint16_t *) ip, sizeof(struct iphdr));      // then calculate real checksum and store it
    }

    void setIcmpHeader(int type, int code, int id, int sequence) {
        icmp->type              = type;
        icmp->code              = code;
        icmp->un.echo.id        = htons(id);
        icmp->un.echo.sequence  = htons(sequence);

        icmp->checksum = 0;                                                 // first set checksum to zero
        icmp->checksum = checksum((uint16_t *) icmp, sizeof(struct icmphdr));   // then calculate real checksum and store it

        echoId = id;
    }

    static uint16_t checksum(uint16_t *addr, int len) {
        int sum         = 0;
        uint16_t answer  = 0;
        uint16_t *w      = addr;
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
    uint8_t*           data;

    uint16_t            echoId;
};

//-------------------------------------

#define MAX_STING_DGRAMS    32

class IcmpWrapper
{
public:
     IcmpWrapper (void);
    ~IcmpWrapper (void);

    void  receiveAll   (void);
    uint8_t  send         (uint32_t destinIP, int icmpType, int icmpCode, uint16_t length, uint8_t *data);

    void  closeAndClean(void);

    uint32_t calcDataByteCountTotal(void);
    int   calcHowManyDatagramsFitIntoBuffer(int bufferSizeBytes);

    int   getNonEmptyIndex(void);
    TStingDgram      dgrams[MAX_STING_DGRAMS];

private:
    uint8_t            *recvBfr;
    uint32_t            icmpDataCount;

    TRawSocks        rawSockHeads;      // this holds the headers for RAW socket
    TNetConnection  *rawSock;           // this is info about RAW socket - used for ICMP

    bool  receive       (void);
    void  clearOld      (void);
    int   getEmptyIndex (void);
};

#endif
