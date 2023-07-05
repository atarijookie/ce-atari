#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

#define PORT 15000

int main(int argc, char *argv[])
{
    #define MSG_SIZE    660
    char msg[MSG_SIZE];

    //setup a socket and connection tools
    struct hostent* host = gethostbyname("192.168.123.55");      // 127.0.0.1 - 1189 kB/s

    sockaddr_in sendSockAddr;
    bzero((char*)&sendSockAddr, sizeof(sendSockAddr));

    sendSockAddr.sin_family = AF_INET;
    sendSockAddr.sin_addr.s_addr = inet_addr(inet_ntoa(*(struct in_addr*)*host->h_addr_list));
    sendSockAddr.sin_port = htons(PORT);
    int clientSd = socket(AF_INET, SOCK_STREAM, 0);

    int status = connect(clientSd, (sockaddr*) &sendSockAddr, sizeof(sendSockAddr));
    if(status < 0)
    {
        printf("Error connecting to socket!\n");
        return -1;
    }

    printf("Connected to the server.\n");
    int bytesRead, bytesWritten = 0;
    struct timeval start1, end1;

    for(int i=0; i<MSG_SIZE; i++) {
        msg[i] = i;
    }

    while(1) {
        send(clientSd, (char*)&msg, MSG_SIZE, 0);
        usleep(437);        // 437 us between packets -> 2288 packets/s -> 512 B data in packet -> 1144 kB/s
    }

    close(clientSd);
    return 0;
}