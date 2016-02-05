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

int createListeningSocket(int port);
int acceptConnection(int listenFd);

#define LISTEN_PORT_START   10000

#define MAX_FDS  4
int fdListen[MAX_FDS];
int fdData  [MAX_FDS];

void clientMain(void);
void serverMain(void);

void sleepMs(int ms);
int  getCurrentMs(void);

void server_checkListeningSocketsAndAccept(void);
void server_checkIfDataSocketsClosed(void);
void server_readWriteData(void);

//---------------------------------
void handleSocket0(int readCount);

void handleSocket1(int readCount);
void sock1send (void);
void sock1close(void);

void handleSocket2(int readCount);

void handleSocket3(int readCount);
void closeSock3(void);
//---------------------------------

/*
Server sockets:
+0 - echo - returns the same as you send there
+1 - client sends block length, pause between blocks, block count
+2 - client sends number of lines, server sends that count of lines of text (terminated with end-of-line)
+3 - client sends time, close socket after that time seconds
*/

#define BFR_SIZE        (1024 * 1024)
unsigned char gBfrOut[BFR_SIZE];
unsigned char gBfrIn[BFR_SIZE];

int main(int argc, char *argv[])
{
    printf("\n\n");

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
}

//----------------------------------------

struct {
    int configReceived;

    int blockCount;
    int blockLength;
    int pauseBetweenBlocks;

    int lastSendTime;
} sock1conf;

struct {
    int configReceived;
    int closeAfterTime;
    int closeTime;
} sock3conf;

void serverMain(void)
{
    printf("Running as server...\n");

    int i;
    for(i=0; i<MAX_FDS; i++) {
        fdListen[i] = createListeningSocket(LISTEN_PORT_START + i);
    }
    
    int loops = 0;
    while(1) {
        server_checkListeningSocketsAndAccept();
        server_checkIfDataSocketsClosed();

        server_readWriteData();

        if(loops >= 10) {
            printf(".\n");
            loops = 0;
        }
        loops++;

        sleepMs(100);
    }

    printf("Server has terminated.\n");
}

//----------------------------------------

// socket 0: echo socket
void handleSocket0(int readCount)
{
    int rcnt = (readCount < BFR_SIZE) ? readCount : BFR_SIZE;

    int res  = read(fdData[0], gBfrIn, rcnt);               // read

    if(res == 0) {                                          // closed?
        printf("Socket 0 closed by other side.\n");

        close(fdData[0]);
        fdData[0] = 0;
    }

    if(res < 0) {                                           // fail?
        return;
    }

    write(fdData[0], gBfrIn, res);                          // write back
}

void handleSocket1(int readCount)
{
    int rcnt = (readCount < BFR_SIZE) ? readCount : BFR_SIZE;

    int res  = read(fdData[1], gBfrIn, rcnt);               // read

    if(res == 0) {                                          // closed?
        printf("Socket 1 closed by other side.\n");
        sock1close();
        return;
    }

    if(res < 0) {                                           // fail?
        return;
    }

    if(!sock1conf.configReceived) {
        sock1conf.blockCount            = (gBfrIn[0] <<  8) |  gBfrIn[1];
        sock1conf.blockLength           = (gBfrIn[2] << 24) | (gBfrIn[3] << 16) | (gBfrIn[4] << 8) | gBfrIn[5];
        sock1conf.pauseBetweenBlocks    = (gBfrIn[6] <<  8) |  gBfrIn[7];

        sock1conf.configReceived    = 1;
        sock1conf.lastSendTime      = 0;
    }
}

void sock1close(void)
{
    if(fdData[1] > 0) {
        close(fdData[1]);
        fdData[1] = 0;
    }

    sock1conf.configReceived    = 0;
    sock1conf.lastSendTime      = 0;
}

void sock1send(void)
{
    if(sock1conf.blockCount <= 0) {                 // nothing to send? quit
        return;
    }

    int bl = (sock1conf.blockLength < BFR_SIZE) ? sock1conf.blockLength : BFR_SIZE;

    int cntr = 0;
    int i;

    for(i=0; i<bl;) {                               // fill buffer with 16 bit counter
        gBfrOut[i++] = (unsigned char) (cntr >> 8);
        gBfrOut[i++] = (unsigned char) (cntr     );
        cntr++;
    }

    write(fdData[1], gBfrOut, bl);                  // send data
    sock1conf.blockCount--;
}

void handleSocket2(int readCount)
{
    if(readCount < 2) {
        return;
    }

    int res = read(fdData[2], gBfrIn, 2);           // read

    if(res <= 0) {                                  // closed or fail?
        goto closeSock2;
    }

    int linesCount = (gBfrIn[0] <<  8) |  gBfrIn[1];

    char *lines[10] = {
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
    
    int i, lineIndex = 0;
    for(i=0; i<linesCount; i++) {               // send all required lines to client
        char *line = lines[lineIndex];          // get line

        int len = strlen(line);
        char tmp[32];
        sprintf(tmp, "%04d", len);
        write(fdData[2], tmp,  4);              // send length

        write(fdData[2], line, len);            // send data

        lineIndex++;
        if(lineIndex > 9) {                     // move to next line
            lineIndex = 0;
        }
    }


// close the socket
closeSock2:
    close(fdData[2]);
    fdData[2] = 0;
    printf("Socket 2 closed\n");
}

void handleSocket3(int readCount)
{
    if(readCount < 2) {                 // not enough data?
        return;
    }

    if(sock3conf.configReceived) {      // if config received, don't receive it again
        return;
    }

    int res = read(fdData[3], gBfrIn, 2);   // read

    if(res <= 0) {                      // closed or fail?
        closeSock3();
        return;
    }

    sock3conf.closeAfterTime    = (gBfrIn[0] <<  8) |  gBfrIn[1];               // time - in ms, after which the socket should be closed
    sock3conf.closeTime         = getCurrentMs() + sock3conf.closeAfterTime;    // local time, after which the sock should be closed
    sock3conf.configReceived    = 1;
}

void closeSock3(void)
{
    if(fdData[3] > 0) {
        close(fdData[3]);
        fdData[3] = 0;
    }

    sock3conf.configReceived    = 0;
    sock3conf.closeAfterTime    = 0;
    sock3conf.closeTime         = 0;
}

void server_readWriteData(void)
{
    int i;
    for(i=0; i<MAX_FDS; i++) {
        if(fdData[i] > 0) {             // if socket connected
            int count;
            int res = ioctl(fdData[i], FIONREAD, &count);

            if(res == 0 && count > 0) {
                switch(i) {
                    case 0: handleSocket0(count); break;
                    case 1: handleSocket1(count); break;
                    case 2: handleSocket2(count); break;
                    case 3: handleSocket3(count); break;
                }
            }
        }
    }

    //-------------
    if(fdData[1] != 0 && sock1conf.configReceived) {            // if got socket 1 and got also the config for it
        int now             = getCurrentMs();
        int nextSendTime    = sock1conf.lastSendTime + sock1conf.pauseBetweenBlocks;

        if(now >= nextSendTime && sock1conf.blockCount > 0) {   // if it's time to send something, and we still should send a block
            sock1conf.lastSendTime = now;

            sock1send();
        }

        if(sock1conf.blockCount <= 0) {                         // nothing more to send? close socket
            sock1close();
        }
    }

    //-------------
    if(fdData[3] != 0 && sock3conf.configReceived) {            // if got socket 3 and got also the config for it
        int now = getCurrentMs();

        if(now >= sock3conf.closeTime) {                        // if enough time passed, we can close this socket
            closeSock3();
        }
    }
}

void server_checkListeningSocketsAndAccept(void)
{
    int i;

    for(i=0; i<MAX_FDS; i++) {
        if(fdData[i] < 1) {             // if connection is not accept()ed yet
            int s = acceptConnection(fdListen[i]);

            if(s > 0) {
                fdData[i] = s;
                printf("Socked %d connected.\n", i);
            }
        }
    }
}

void server_checkIfDataSocketsClosed(void)
{
    int i;
    char c;
    ssize_t res;

    for(i=0; i<MAX_FDS; i++) {
        if(fdData[i] > 0) {             // if socket opened
            res = recv(fdData[i], &c, 1, MSG_PEEK);

            if(res == 0) {
                printf("Socket %d closed by other side.\n", i);

                if(i == 1) {            // socket 1? close in special way
                    sock1close();
                } else if(i == 3) {     // socket 3? close in special way
                    closeSock3();
                } else {                // other socket? close normally
                    close(fdData[i]);
                    fdData[i] = 0;
                }
            }

            if(res == -1) {
                if(errno == EAGAIN || errno == EWOULDBLOCK) {
                
                }
            }
        }
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
    int ires  = fcntl(fd, F_SETFL, flags | O_NONBLOCK);             // set it as non-blocking

    return fd;
}

int createListeningSocket(int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);                       // create socket
    
    int allowReuse = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&allowReuse, sizeof(allowReuse));

    struct sockaddr_in serv_addr; 
    memset(&serv_addr,  0, sizeof(serv_addr));

    serv_addr.sin_family        = AF_INET;
    serv_addr.sin_addr.s_addr   = htonl(INADDR_ANY);
    serv_addr.sin_port          = htons(port);                      // port

    bind(fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));      // bind socket

    listen(fd, 1);                                                  // set length of listening queue (max waiting connections)

    int flags = fcntl(fd, F_GETFL, 0);                              // get flags
    int ires  = fcntl(fd, F_SETFL, flags | O_NONBLOCK);             // set it as non-blocking

    return fd;
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

void testSock0(void)
{
    printf("\n\ntestSock0() running...\n");

    int s = connectToHost("127.0.0.1", LISTEN_PORT_START);

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

    write(s, gBfrOut, ECHO_SIZE);                               // send output buffer

    int gotBytes;
    memset(gBfrIn, 0, ECHO_SIZE);
    gotBytes = readLoop(s, gBfrIn, ECHO_SIZE, 5000);            // receive input buffer
    close(s);

    int match = memcmp(gBfrIn, gBfrOut, ECHO_SIZE);
    printf("testSock0: Sent %d bytes, received %d bytes, data match: %d\n", ECHO_SIZE, gotBytes, (int) (match == 0));
}

void testSock1(void)
{
    printf("\n\ntestSock1() running...\n");

    int s = connectToHost("127.0.0.1", LISTEN_PORT_START + 1);

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

    write(s, gBfrOut, 8);                                       // send output buffer

    int gotBytes;
    memset(gBfrIn, 0, ECHO_SIZE);
    gotBytes = readLoop(s, gBfrIn, SOCK1_BLOCKCOUNT * SOCK1_BLOCKLENGTH, (SOCK1_BLOCKCOUNT + 1) * SOCK1_BLOCKPAUSE);    // receive input buffer
    close(s);

    printf("testSock1: received %d bytes, wanted %d bytes\n", gotBytes, SOCK1_BLOCKCOUNT * SOCK1_BLOCKLENGTH);
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

void testSock2(void)
{
    printf("\n\ntestSock2() running...\n");

    int s = connectToHost("127.0.0.1", LISTEN_PORT_START + 2);

    if(s <= 0) {
        return;
    }

    #define SOCK2_LINE_COUNT    20

    gBfrOut[0] = (unsigned char) (SOCK2_LINE_COUNT >> 8);
    gBfrOut[1] = (unsigned char) (SOCK2_LINE_COUNT     );

    write(s, gBfrOut, 2);                                   // send output buffer

    int gotBytes;
    memset(gBfrIn, 0, BFR_SIZE);
    gotBytes = readLoop(s, gBfrIn, BFR_SIZE, 3000);         // receive input buffer
    close(s);

    int gotLines = 0;
    int linesGood = 0, linesBad = 0;

    char *pBfr = gBfrIn;
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

    printf("testSock2: wanted %d lines, got %d lines. Good: %d, Bad: %d\n", SOCK2_LINE_COUNT, gotLines, linesGood, linesBad);
}

void testSock3(void)
{
    printf("\n\ntestSock3() running...\n");

    int s = connectToHost("127.0.0.1", LISTEN_PORT_START + 3);

    if(s <= 0) {
        return;
    }

    #define SOCK3_CLOSETIME    3000

    gBfrOut[0] = (unsigned char) (SOCK3_CLOSETIME >> 8);
    gBfrOut[1] = (unsigned char) (SOCK3_CLOSETIME     );
    write(s, gBfrOut, 2);                                       // send output buffer

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

    printf("testSock3: socket should have closed after %d ms, did close after %d ms\n", SOCK3_CLOSETIME, diff);
}

void clientMain(void)
{
    printf("Running as client...\n");

    testSock0();
    testSock1();
    testSock2();
    testSock3();

    printf("Client has terminated.\n");
}

int connectToHost(char *host, int port)
{
    struct sockaddr_in serv_addr; 

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if(s < 0) {
        return -1;
    } 

    // turn off Nagle's algo for lower latencies (but also lower throughput)
    int tcpNoDelay = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char*)&tcpNoDelay, sizeof(int));

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

