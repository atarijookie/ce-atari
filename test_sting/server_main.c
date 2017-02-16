// vim: expandtab shiftwidth=4 tabstop=4
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h> 
#include <fcntl.h>
#include <sys/ioctl.h>

int createListeningSocket(int port, int tcpNotUdp);
int acceptConnection(int listenFd);
int connectToHost(char *host, int port, int tcpNotUdp);

#define LISTEN_PORT_START   10000

#define MAX_FDS  8
int fdListen[MAX_FDS];
int fdData  [MAX_FDS];

int fdListen20;
int fdData20;

#define IS_TCP_SOCK(INDEX)  (INDEX >= 0 && INDEX < 4)
#define IS_UDP_SOCK(INDEX)  (INDEX >= 4 && INDEX < MAX_FDS)

void clientMain(void);
void serverMain(void);

void sleepMs(int ms);
int  getCurrentMs(void);

void server_checkListeningSocketsAndAccept(void);
void server_checkIfDataSocketsClosed(void);
void server_readWriteData(void);

//---------------------------------

typedef struct {
    int configReceived;

    int blockCount;
    int blockLength;
    int pauseBetweenBlocks;

    int lastSendTime;

    struct sockaddr_in si_other;
    socklen_t          slen;
} Tsock1conf;

typedef struct {
    int configReceived;
    int closeAfterTime;
    int closeTime;
} Tsock3conf;

Tsock1conf sock1conf, sock5conf;
Tsock3conf sock3conf, sock7conf;

//---------------------------------

void handleSocket0(int *dataFd, int port, int readCount, int tcpNotUdp);

void handleSocket1(int *dataFd, int port, Tsock1conf *sc, int readCount, int tcpNotUdp);
void sock1send    (int *dataFd, int port, Tsock1conf *sc,                int tcpNotUdp);
void sock1close   (int *dataFd, int port, Tsock1conf *sc,                int tcpNotUdp);
void handleSock1  (int *dataFd, int port, Tsock1conf *sc,                int tcpNotUdp);

void handleSocket2(int *dataFd, int port, int readCount, int tcpNotUdp);

void handleSocket3(int *dataFd, int port, Tsock3conf *sc, int readCount, int tcpNotUdp);
void closeSock3   (int *dataFd, int port, Tsock3conf *sc,                int tcpNotUdp);
void handleSock3  (int *dataFd, int port, Tsock3conf *sc,                int tcpNotUdp);

void handleSocket20(void);

//---------------------------------

// utility function : write all bytes to the socket

static ssize_t write_all(int fd, const void * buf, size_t nbytes)
{
    ssize_t res;
    const char * p = (const char *)buf;
    ssize_t written = 0;

    do {
        res = write(fd, p, nbytes);
        if(res < 0) {
            if(errno == EINTR) continue;    // interrupted : try again
            if(errno == EAGAIN || errno == EWOULDBLOCK) continue;
            fprintf(stderr, "write_all() failed after %d bytes : %s\n", (int)written, strerror(errno));
            return res;
        } else if(res == 0) {
            fprintf(stderr, "write_all() closed after %d bytes\n", (int)written);
            return res;
        }
        nbytes -= res;
        written += res;
        p += res;
    } while(nbytes > 0);

    return written;
}

//---------------------------------

/*

Server TCP sockets:
+0 - echo - returns the same as you send there
+1 - client sends block length, pause between blocks, block count
+2 - client sends number of lines, server sends that count of lines of text (terminated with end-of-line)
+3 - client sends time, close socket after that time seconds

+4 \
+5  \ Server UDP sockets, same functionality as TCP, but on higher port #
+6  /
+7 /

+20 - TCP socket, which waits for client IP and port, so it can then connect +0 to that

*/

#define BFR_SIZE        (1024 * 1024)
unsigned char gBfrOut[BFR_SIZE];
unsigned char gBfrIn[BFR_SIZE];

int main(int argc, char *argv[])
{
	printf("usage: %s [server]\ndefault is client mode\n\n", argv[0]);

    int asClient = 1;
    if(argc > 1) {
        if(strcmp(argv[1], "server") == 0) {
            asClient = 0;
        }
    }

    if(asClient) {
        clientMain();
    } else {
        serverMain();
    }
	return 0;
}

//----------------------------------------

void serverMain(void)
{
    printf("Running as server...\n");

    int i;
    for(i=0; i<MAX_FDS; i++) {
        if(IS_TCP_SOCK(i)) {
            fdListen[i] = createListeningSocket(LISTEN_PORT_START + i, 1);      // 0..3: TCP
        } else {
            fdData[i]   = createListeningSocket(LISTEN_PORT_START + i, 0);      // 4..7: UDP
        }
    }

    fdListen20 = createListeningSocket(LISTEN_PORT_START + 20, 1);              // +20: TCP
    
    int loops = 0;
    while(1) {
        server_checkListeningSocketsAndAccept();    // does nothing for UDP
        server_checkIfDataSocketsClosed();          // does nothing for UDP

        server_readWriteData();

        if(loops >= 10) {
            //printf(".\n");
            loops = 0;
        }
        loops++;

        sleepMs(100);
    }

    printf("Server has terminated.\n");
}

//----------------------------------------

// socket 0: echo socket
void handleSocket0(int *dataFd, int port, int readCount, int tcpNotUdp)
{
	(void)port;
    int rcnt = (readCount < BFR_SIZE) ? readCount : BFR_SIZE;

    int res;
    
    struct sockaddr_in si_other;
    socklen_t          slen = sizeof(si_other);

    // read
    if(tcpNotUdp) {     // TCP?
        res = read     (*dataFd, gBfrIn, rcnt);                 

        if(res == 0) {                                          // closed?
            printf("handleSocket0 - closed by other side.\n");

            close(*dataFd);
            *dataFd = 0;
        }
    } else {            // UDP?
        res = recvfrom(*dataFd, gBfrIn, rcnt, 0, (struct sockaddr *) &si_other, &slen);
    }

    if(res < 0) {                                           // fail?
        printf("handleSocket0 - read failed\n");
        return;
    }

    printf("handleSocket0 - did read %d bytes\n", res);
    
    // write back
    if(tcpNotUdp) {
        write_all (*dataFd, gBfrIn, res);
    } else {
        sendto(*dataFd, gBfrIn, res, 0, (struct sockaddr*) &si_other, slen);
    }
}

void handleSocket1(int *dataFd, int port, Tsock1conf *sc, int readCount, int tcpNotUdp)
{
    int rcnt = (readCount < BFR_SIZE) ? readCount : BFR_SIZE;

    int res;

    if(tcpNotUdp) {     // TCP?
        res  = read(*dataFd, gBfrIn, rcnt);                     // read

        if(res == 0) {                                          // closed?
            printf("Socket 1 closed by other side.\n");
            sock1close(dataFd, port, sc, tcpNotUdp);
            return;
        }
    } else {            // UDP?
        sc->slen    = sizeof(sc->si_other);
        res         = recvfrom(*dataFd, gBfrIn, rcnt, 0, (struct sockaddr *) &sc->si_other, &sc->slen);
    }

    if(res < 0) {                                           // fail?
        return;
    }

    if(!sc->configReceived) {
        sc->blockCount            = (gBfrIn[0] <<  8) |  gBfrIn[1];
        sc->blockLength           = (gBfrIn[2] << 24) | (gBfrIn[3] << 16) | (gBfrIn[4] << 8) | gBfrIn[5];
        sc->pauseBetweenBlocks    = (gBfrIn[6] <<  8) |  gBfrIn[7];

        sc->configReceived    = 1;
        sc->lastSendTime      = 0;
    }
}

void sock1close(int *dataFd, int port, Tsock1conf *sc, int tcpNotUdp)
{
	(void)port;

    if(*dataFd > 0 && tcpNotUdp) {
        close(*dataFd);
        *dataFd = 0;
    }

    sc->configReceived    = 0;
    sc->lastSendTime      = 0;
}

void sock1send(int *dataFd, int port, Tsock1conf *sc, int tcpNotUdp)
{
	(void)port;

    if(sc->blockCount <= 0) {                 // nothing to send? quit
        return;
    }

    int bl = (sc->blockLength < BFR_SIZE) ? sc->blockLength : BFR_SIZE;

    int cntr = 0;
    int i;

    for(i=0; i<bl;) {                               // fill buffer with 16 bit counter
        gBfrOut[i++] = (unsigned char) (cntr >> 8);
        gBfrOut[i++] = (unsigned char) (cntr     );
        cntr++;
    }

    if(tcpNotUdp) {         // TCP?
        write_all (*dataFd, gBfrOut, bl);                  // send data
    } else {                // UDP?
        sendto(*dataFd, gBfrOut, bl, 0, (struct sockaddr*) &sc->si_other, sc->slen);
    }

    sc->blockCount--;
}

void handleSocket2(int *dataFd, int port, int readCount, int tcpNotUdp)
{
    int i;
    size_t len;
    (void)port;

    if(readCount < 2) {
        printf("handleSocket2 - not enough data\n");
        return;
    }

    int res;
    struct sockaddr_in si_other;
    socklen_t          slen = sizeof(si_other);

    // read
    if(tcpNotUdp) {     // TCP?
        res = read    (*dataFd, gBfrIn, 2);             // read
    } else {
        res = recvfrom(*dataFd, gBfrIn, 2, 0, (struct sockaddr *) &si_other, &slen);
    }

    if(res <= 0) {                                  // closed or fail?
        printf("handleSocket2 - closed or fail\n");
        goto closeSock2;
    }

    int linesCount = (gBfrIn[0] <<  8) |  gBfrIn[1];

    char bigBuf[100*1024];
    memset(bigBuf, 0, 100*1024);
    
    printf("handleSocket2 - will send linesCount: %d\n", linesCount);
    
    static const char *lines[10] = {
                        "Bacon ipsum dolor amet strip steak turducken meatball short loin rump ham ribeye ham hock turkey.\n", 
                        "Fatback shank turducken, drumstick chuck turkey pork belly prosciutto.\n",
                        "Beef ribs swine bresaola landjaeger tri-tip kevin rump meatball ground round shankle strip steak beef boudin filet mignon pork chop.\n",
                        "Tail turkey hamburger pork salami pastrami porchetta landjaeger pork loin pig spare ribs drumstick.\n",
                        "Ribeye pancetta meatloaf filet mignon, chicken ham hock sausage meatball spare ribs beef ribs venison capicola t-bone sirloin.\n",
                        "Ground round flank shank cupim.\n",
                        "Alcatra andouille hamburger, fatback flank sausage jowl tail doner rump filet mignon ham cow prosciutto.\n",
                        "Sausage filet mignon tail landjaeger turducken.\n",
                        "Jerky bacon porchetta meatball shoulder landjaeger.\n",
                        "Pig cow turducken bacon beef frankfurter.\n"
                       };

    len = 0;
    for(i=0; i<linesCount; i++) {               // send all required lines to client
        int n;
        const char *line = lines[i % 10];          // get line
        n = snprintf(bigBuf + len, sizeof(bigBuf) - len, "%04u%s", (unsigned)strlen(line), line);
        if(n >= (int)sizeof(bigBuf) - (int)len) break;
        len += n;
    }
    printf("sending %d lines %u bytes\n", i, (unsigned)len);

    // send in one big send, because otherwise Sting somehow fails to receive all the data...
    if(tcpNotUdp) {                         // TCP?
        write_all (*dataFd, bigBuf,  len); // send length
    } else {
        sendto(*dataFd, bigBuf,  len, 0, (struct sockaddr*) &si_other, slen);
    }

printf("Data sent: %lu (0x%lx)\n", len, len);

// close the socket
closeSock2:
    if(tcpNotUdp) {         // TCP?
        close(*dataFd);
        *dataFd = 0;
    }

    printf("Socket 2 closed\n\n");
}

void handleSocket3(int *dataFd, int port, Tsock3conf *sc, int readCount, int tcpNotUdp)
{
    if(readCount < 2) {                 // not enough data?
        printf("handleSocket3 - not enough data\n");
        return;
    }

    if(sc->configReceived) {            // if config received, don't receive it again
        printf("handleSocket3 - config received, not processing\n");
        return;
    }

    int res;
    struct sockaddr_in si_other;
    socklen_t          slen = sizeof(si_other);

    // read
    if(tcpNotUdp) {     // TCP?
        res = read    (*dataFd, gBfrIn, 2); // read
    } else {            // UDP?
        res = recvfrom(*dataFd, gBfrIn, 2, 0, (struct sockaddr *) &si_other, &slen);
    }

    if(res <= 0) {                      // closed or fail?
        closeSock3(dataFd, port, sc, tcpNotUdp);
        return;
    }

    sc->closeAfterTime    = (gBfrIn[0] <<  8) |  gBfrIn[1];               // time - in ms, after which the socket should be closed
    sc->closeTime         = getCurrentMs() + sock3conf.closeAfterTime;    // local time, after which the sock should be closed
    sc->configReceived    = 1;
    
    printf("handleSocket3 - closeAfterTime: %d ms\n", sc->closeAfterTime);
}

void closeSock3(int *dataFd, int port, Tsock3conf *sc, int tcpNotUdp)
{
	(void)port;

    if(*dataFd > 0 && tcpNotUdp) {
        printf("closeSock3 - closed socket %d\n", *dataFd);
        close(*dataFd);
        *dataFd = 0;
    } else {
        printf("closeSock3 - NOT closed socket %d\n", *dataFd);
    }

    sc->configReceived    = 0;
    sc->closeAfterTime    = 0;
    sc->closeTime         = 0;
}

void server_readWriteData(void)
{
    int count;
    int res;
    int i;

    for(i=0; i<MAX_FDS; i++) {
        if(fdData[i] > 0) {             // if socket connected
            res = ioctl(fdData[i], FIONREAD, &count);

            int tcpNotUdp = IS_TCP_SOCK(i);

            if(count > 0) {
                printf("server_readWriteData: got data for socket %d - %d bytes\n", i, count);
            }

            if(res == 0 && count > 0) {
                switch(i) {
                    case 4:
                    case 0: handleSocket0(&fdData[i], LISTEN_PORT_START + i,             count, tcpNotUdp); break;

                    case 1: handleSocket1(&fdData[i], LISTEN_PORT_START + i, &sock1conf, count, tcpNotUdp); break;
                    case 5: handleSocket1(&fdData[i], LISTEN_PORT_START + i, &sock5conf, count, tcpNotUdp); break;

                    case 6:
                    case 2: handleSocket2(&fdData[i], LISTEN_PORT_START + i,             count, tcpNotUdp); break;

                    case 3: handleSocket3(&fdData[i], LISTEN_PORT_START + i, &sock3conf, count, tcpNotUdp); break;
                    case 7: handleSocket3(&fdData[i], LISTEN_PORT_START + i, &sock7conf, count, tcpNotUdp); break;
                }
            }
        }
    }

    //-------------
    // if got socket 1 and got also the config for it
    if(fdData[1] != 0 && sock1conf.configReceived) {            
        handleSock1(&fdData[1], LISTEN_PORT_START + 1, &sock1conf, IS_TCP_SOCK(1));
    }

    if(fdData[5] != 0 && sock5conf.configReceived) {            
        handleSock1(&fdData[5], LISTEN_PORT_START + 5, &sock5conf, IS_TCP_SOCK(5));
    }
    //-------------
    if(fdData[3] != 0 && sock3conf.configReceived) {            // if got socket 3 and got also the config for it
        handleSock3(&fdData[3], LISTEN_PORT_START + 3, &sock3conf, IS_TCP_SOCK(3));
    }

    if(fdData[7] != 0 && sock7conf.configReceived) {            // if got socket 3 and got also the config for it
        handleSock3(&fdData[7], LISTEN_PORT_START + 7, &sock7conf, IS_TCP_SOCK(7));
    }

    //-----------------
    // specialy for socket #20
    res = ioctl(fdData20, FIONREAD, &count);

    if(count >= 6) {
        printf("server_readWriteData: got data for socket 20\n");
        handleSocket20();
    }

}

void handleSock1(int *dataFd, int port, Tsock1conf *sc, int tcpNotUdp)
{
    int now             = getCurrentMs();
    int nextSendTime    = sc->lastSendTime + sc->pauseBetweenBlocks;

    if(now >= nextSendTime && sc->blockCount > 0) {             // if it's time to send something, and we still should send a block
        sc->lastSendTime = now;

        sock1send(dataFd, port, sc, tcpNotUdp);
    }

    if(sc->blockCount <= 0) {                                   // nothing more to send? close socket
        sock1close(dataFd, port, sc, tcpNotUdp);
    }
}

void handleSock3(int *dataFd, int port, Tsock3conf *sc, int tcpNotUdp)
{
    int now = getCurrentMs();

    if(now >= sc->closeTime) {                                  // if enough time passed, we can close this socket
        printf("handleSock3 - closing socket 3, because %d >= %d\n", now, sc->closeTime);
        closeSock3(dataFd, port, sc, tcpNotUdp);
    }
}

void server_checkListeningSocketsAndAccept(void)
{
    int i;

    for(i=0; i<MAX_FDS; i++) {
        if(IS_UDP_SOCK(i)) {                                    // skip UDP sockets
            continue;
        }

        if(fdData[i] < 1 && fdListen[i] > 0) {                  // if TCP connection is not accept()ed yet
            int s = acceptConnection(fdListen[i]);

            if(s > 0) {
                fdData[i] = s;
                printf("Socked %d connected.\n", i);
            }
        }
    }

    if(fdListen20 > 0) {
        int s = acceptConnection(fdListen20);
        
        if(s > 0) {
            fdData20 = s;
            printf("Socked 20 connected.\n");
        }
    }
}

void server_checkIfDataSocketsClosed(void)
{
    int i;
    char c;
    ssize_t res;

    for(i=0; i<MAX_FDS; i++) {
        if(IS_UDP_SOCK(i)) {            // skip UDP sockets
            continue;
        }

        if(fdData[i] > 0) {             // if socket opened
            res = recv(fdData[i], &c, 1, MSG_PEEK);

            int tcpNotUdp = IS_TCP_SOCK(i);

            if(res == 0) {
                printf("Socket %d closed by other side.\n", i);

                if       (i == 1) {     // socket 1? close in special way
                    sock1close(&fdData[i], LISTEN_PORT_START + i, &sock1conf, tcpNotUdp);
                } else if(i == 5) {
                    sock1close(&fdData[i], LISTEN_PORT_START + i, &sock5conf, tcpNotUdp);
                } else if(i == 3) {     // socket 3? close in special way
                    closeSock3(&fdData[i], LISTEN_PORT_START + i, &sock3conf, tcpNotUdp);
                } else if(i == 3) {     // socket 3? close in special way
                    closeSock3(&fdData[i], LISTEN_PORT_START + i, &sock7conf, tcpNotUdp);
                } else {                // other socket? close normally
                    if(tcpNotUdp) {
                        close(fdData[i]);
                        fdData[i] = 0;
                    }
                }
            }

            if(res == -1) {
                if(errno == EAGAIN || errno == EWOULDBLOCK) {
                
                }
            }
        }
    }

    if(recv(fdData20, &c, 1, MSG_PEEK) == 0) {
        close(fdData20);
        fdData20 = 0;
    }
}

int acceptConnection(int listenFd)
{
    int fd = accept(listenFd, (struct sockaddr*)NULL, NULL); 

    // turn off Nagle's algo for lower latencies (but also lower throughput)
    int tcpNoDelay = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char*)&tcpNoDelay, sizeof(int));

    // set 15 second timeout on read operations to avoid getting stuck forever on some read / recv
    struct timeval tv;
    tv.tv_sec   = 15;  // 15 seconds timeout
    tv.tv_usec  = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval));

    int flags = fcntl(fd, F_GETFL, 0);                              // get flags
    (void)fcntl(fd, F_SETFL, flags | O_NONBLOCK);             // set it as non-blocking

    return fd;
}

int createListeningSocket(int port, int tcpNotUdp)
{
    int fd;

    // create socket
    if(tcpNotUdp) {     // for TCP
        fd = socket(AF_INET, SOCK_STREAM, 0);
    } else {            // for UDP
        fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    }
    
    int allowReuse = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&allowReuse, sizeof(allowReuse));

    struct sockaddr_in serv_addr; 
    memset(&serv_addr,  0, sizeof(serv_addr));

    serv_addr.sin_family        = AF_INET;
    serv_addr.sin_addr.s_addr   = htonl(INADDR_ANY);
    serv_addr.sin_port          = htons(port);                      // port

    bind(fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));      // bind socket

    if(tcpNotUdp) {     // for TCP
        listen(fd, 1);                                              // set length of listening queue (max waiting connections)
    }

    int flags = fcntl(fd, F_GETFL, 0);                              // get flags
    (void)fcntl(fd, F_SETFL, flags | O_NONBLOCK);             // set it as non-blocking

    printf("Created %s socket on port %d (fd = %d)\n", tcpNotUdp ? "TCP" : "UDP", port, fd);
    return fd;
}

void handleSocket20(void)
{
    int res;

    res = read(fdData20, gBfrIn, 6);    // read

    if(res <= 0) {                      // closed or fail?
        goto closeSocket20;
    }

    int ip1,ip2,ip3,ip4, port;          // get IP and port, convert it to string
    ip1 = gBfrIn[0];
    ip2 = gBfrIn[1];
    ip3 = gBfrIn[2];
    ip4 = gBfrIn[3];

    port = (((int) gBfrIn[4]) << 8) | ((int) gBfrIn[5]);

    char host[32];
    sprintf(host, "%d.%d.%d.%d", ip1, ip2, ip3, ip4);
    
    int s = connectToHost(host, port, 1);

    if(s <= 0) {                        // if failed, quit
        goto closeSocket20;
    }

    if(fdData[0]) {                     // if there was some socket +0 opened, close it
        close(fdData[0]);
    }

    fdData[0] = s;                      // pretend, that this outgoing socket is incomming ECHO (+0) socket

closeSocket20:
    close(fdData20);                    // close socket #20, it's not needed anymore
    fdData20 = 0;
}

/////////////////////////////////////////////////////////////////////

int readLoop(int fd, char *bfrIn, int cnt, int timeoutMs)
{
    int gotBytes = 0;
    int timeEnd = getCurrentMs() + timeoutMs;

    while(gotBytes < cnt) {                     // while we didn't read all what we needed
        int now = getCurrentMs();
        if(now >= timeEnd) {                    // if timeout passed, quit
            break;
        }

        int cntRest = cnt - gotBytes;
        int res = read(fd, bfrIn, cntRest);     // read

        if(res > 0) {                           // some bytes were read
            gotBytes    += res;
            bfrIn       += res;
        } else {
            sleepMs(1);
        }
    }

    return gotBytes;
}

void testSock0(int portOffset, int tcpNotUdp)
{
    printf("\n\ntestSock%d() running...\n", portOffset);

    int s = connectToHost("127.0.0.1", LISTEN_PORT_START + portOffset, tcpNotUdp);

    if(s <= 0) {
        return;
    }

    #define ECHO_SIZE   10000

    int i, cntr = 0;
    for(i=0; i<ECHO_SIZE;) {                                    // fill output buffer
        gBfrOut[i++] = (unsigned char) (cntr >> 8);
        gBfrOut[i++] = (unsigned char) (cntr     );
        cntr++;
    }

    write_all(s, gBfrOut, ECHO_SIZE);                               // send output buffer

    int gotBytes;
    memset(gBfrIn, 0, ECHO_SIZE);
    gotBytes = readLoop(s, (char *)gBfrIn, ECHO_SIZE, 5000);            // receive input buffer
    close(s);

    int match = memcmp(gBfrIn, gBfrOut, ECHO_SIZE);
    printf("testSock%d: Sent %d bytes, received %d bytes, data match: %d\n", portOffset, ECHO_SIZE, gotBytes, (int) (match == 0));
}

void testSock1(int portOffset, int tcpNotUdp)
{
    printf("\n\ntestSock%d() running...\n", portOffset);

    int s = connectToHost("127.0.0.1", LISTEN_PORT_START + portOffset, tcpNotUdp);

    if(s <= 0) {
        return;
    }

    #define SOCK1_BLOCKCOUNT    5
    #define SOCK1_BLOCKLENGTH   10000
    #define SOCK1_BLOCKPAUSE    1000

    gBfrOut[0] = (unsigned char) (SOCK1_BLOCKCOUNT >> 8);
    gBfrOut[1] = (unsigned char) (SOCK1_BLOCKCOUNT     );

    gBfrOut[2] = (unsigned char) (SOCK1_BLOCKLENGTH >> 24);
    gBfrOut[3] = (unsigned char) (SOCK1_BLOCKLENGTH >> 16);
    gBfrOut[4] = (unsigned char) (SOCK1_BLOCKLENGTH >>  8);
    gBfrOut[5] = (unsigned char) (SOCK1_BLOCKLENGTH      );

    gBfrOut[6] = (unsigned char) (SOCK1_BLOCKPAUSE >> 8);
    gBfrOut[7] = (unsigned char) (SOCK1_BLOCKPAUSE     );

    write_all(s, gBfrOut, 8);                                       // send output buffer

    int gotBytes;
    memset(gBfrIn, 0, ECHO_SIZE);
    gotBytes = readLoop(s, (char *)gBfrIn, SOCK1_BLOCKCOUNT * SOCK1_BLOCKLENGTH, (SOCK1_BLOCKCOUNT + 1) * SOCK1_BLOCKPAUSE);    // receive input buffer
    close(s);

    printf("testSock%d: received %d bytes, wanted %d bytes\n", portOffset, gotBytes, SOCK1_BLOCKCOUNT * SOCK1_BLOCKLENGTH);
}

int getIntFromString(char *s)
{
    int no = 0;
    int i;

    for(i=0; i<4; i++) {
        if(s[i] < '0' || s[i] > '9') {
            break;
        }

        no  = no * 10;
        no += s[i] - '0';
    }

    return no;
}

void printNchars(char *s, int cnt)
{
    int i;

    for(i=0; i<cnt; i++) {
        printf("%c", s[i]);
    }
    printf("\n");
}

void testSock2(int portOffset, int tcpNotUdp)
{
    printf("\n\ntestSock%d() running...\n", portOffset);

    int s = connectToHost("127.0.0.1", LISTEN_PORT_START + portOffset, tcpNotUdp);

    if(s <= 0) {
        return;
    }

    #define SOCK2_LINE_COUNT    20

    gBfrOut[0] = (unsigned char) (SOCK2_LINE_COUNT >> 8);
    gBfrOut[1] = (unsigned char) (SOCK2_LINE_COUNT     );

    write_all(s, gBfrOut, 2);                                   // send output buffer

    int gotBytes;
    memset(gBfrIn, 0, BFR_SIZE);
    gotBytes = readLoop(s, (char *)gBfrIn, BFR_SIZE, 3000);         // receive input buffer
    close(s);
	(void)gotBytes;

    int gotLines = 0;
    int linesGood = 0, linesBad = 0;

    char *pBfr = (char *)gBfrIn;
    while(1) {
        int lineLen = 0;
        char *eol   = strchr(pBfr, '\n');
    
        if(eol) {       // new line found
            lineLen = eol - pBfr;
        } else {        // new line not found
            lineLen = strlen(pBfr);
        }

        if(lineLen == 0) {
            break;
        }

        int lineLen2 = getIntFromString(pBfr);

        if((lineLen2 + 3) == lineLen) {
            linesGood++;
        } else {
            linesBad++;
        }

        gotLines++;
        pBfr += lineLen + 1;
    }

    printf("testSock%d: wanted %d lines, got %d lines. Good: %d, Bad: %d\n", portOffset, SOCK2_LINE_COUNT, gotLines, linesGood, linesBad);
}

void testSock3(int portOffset, int tcpNotUdp)
{
    printf("\n\ntestSock%d() running...\n", portOffset);

    int s = connectToHost("127.0.0.1", LISTEN_PORT_START + portOffset, tcpNotUdp);

    if(s <= 0) {
        return;
    }

    #define SOCK3_CLOSETIME    3000

    gBfrOut[0] = (unsigned char) (SOCK3_CLOSETIME >> 8);
    gBfrOut[1] = (unsigned char) (SOCK3_CLOSETIME     );
    write_all(s, gBfrOut, 2);                                       // send output buffer

    int start           = getCurrentMs();
    int clientTimeOut   = start + (3 * SOCK3_CLOSETIME);

    while(1) {                                  // while we didn't read all what we needed
        int now = getCurrentMs();
        if(now >= clientTimeOut) {              // if timeout passed, quit
            break;
        }

        int res = read(s, gBfrIn, 1);           // read

        if(res == 0) {                          // socket closed?
            break;
        }
    }
    
    int end     = getCurrentMs();
    int diff    = end - start;

    close(s);

    printf("testSock%d: socket should have closed after %d ms, did close after %d ms\n", portOffset, SOCK3_CLOSETIME, diff);
}

void testUdpReceiving(void)
{
    int s = connectToHost("127.0.0.1", LISTEN_PORT_START + 4, 0);

    if(s <= 0) {
        return;
    }

    printf("testUdpReceiving -- sending 3 x 100 bytes (3 datagrams)\n");

    int i, cntr = 0;
    for(i=0; i<100;) {              // fill output buffer
        gBfrOut[i++] = (unsigned char) (cntr >> 8);
        gBfrOut[i++] = (unsigned char) (cntr     );
        cntr++;
    }

    for(i=0; i<3; i++) {
        write_all(s, gBfrOut, 100);     // send output buffer
    }

    sleep(1);                       // wait a second, just to be sure

    struct sockaddr_in si_other;
    socklen_t          slen = sizeof(si_other);
    int                count, res;

    for(i=0; i<3; i++) {
        res = ioctl(s, FIONREAD, &count);

        if(res < 0) {
            printf("testUdpReceiving -- ioctl() failed\n");
            continue;
        }

        printf("testUdpReceiving -- data available: %d\n", count);
        
        if(count > 0) {
            res = recvfrom(s, gBfrIn, count, 0, (struct sockaddr *) &si_other, &slen);

            if(res < 0) {
                printf("testUdpReceiving -- recvfrom() failed\n");
                continue;
            }

            res = memcmp(gBfrIn, gBfrOut, 100);
            if(res != 0) {
                printf("testUdpReceiving -- received data mismatch\n");
            }
        }
    }

    //------------------------
    int c1, c2;

    write_all(s, gBfrOut, 100);     // send output buffer
    sleep(1);
    ioctl(s, FIONREAD, &c1);
    recvfrom(s, gBfrIn, 50, 0, (struct sockaddr *) &si_other, &slen);
    ioctl(s, FIONREAD, &c2);
    
    printf("Send 100, wating %d, read 50, waiting %d.\n", c1, c2);

    close(s);
}

void clientMain(void)
{
    printf("Running as client...\n");

    testSock0(0, 1);
    testSock1(1, 1);
    testSock2(2, 1);
    testSock3(3, 1);

    testSock0(4, 0);
    testSock1(5, 0);
    testSock2(6, 0);
//  testSock3(7, 0);        // don't run test on 7 on UDP - UDP can't be closed, it's useless test

    testUdpReceiving();

    printf("Client has terminated.\n");
}

int connectToHost(char *host, int port, int tcpNotUdp)
{
    struct sockaddr_in serv_addr; 

    printf("connectToHost(%s, %d, %s)\n", host, port, (tcpNotUdp ? "TCP" : "UDP"));
    
    int s;
    if(tcpNotUdp) {     // for TCP
        s = socket(AF_INET, SOCK_STREAM, 0);
    } else {            // for UDP
        s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    }

    if(s < 0) {
        return -1;
    } 

    if(tcpNotUdp) {     // for TCP
        // turn off Nagle's algo for lower latencies (but also lower throughput)
        int tcpNoDelay = 1;
        setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char*)&tcpNoDelay, sizeof(int));
    }

    // set 15 second timeout on read operations to avoid getting stuck forever on some read / recv
    struct timeval tv;
    tv.tv_sec   = 15;  // 15 seconds timeout
    tv.tv_usec  = 0;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval));

    memset(&serv_addr, 0, sizeof(serv_addr)); 

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port); 

    if(inet_pton(AF_INET, host, &serv_addr.sin_addr) <= 0) {
        printf("connectToHost error: inet_pton failed\n");
        close(s);
        return -1;
    } 

    if(connect(s, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("connectToHost error: Socket connect failed\n");
        close(s);
        return -1;
    } 

    printf("connectToHost: connected to %s, port %d\n", host, port);
    return s;
}

//--------------------------------------------------------------

void sleepMs(int ms)
{
	int us = ms * 1000;
	
	usleep(us);
}

int getCurrentMs(void)
{
	struct timespec tp;
	int res;
	
	res = clock_gettime(CLOCK_MONOTONIC, &tp);					// get current time
	
	if(res != 0) {												// if failed, fail
		return 0;
	}
	
	int val = (tp.tv_sec * 1000) + (tp.tv_nsec / 1000000);	    // convert to milli seconds
	return val;
}
