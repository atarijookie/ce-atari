#ifndef _SOCKS_H_
#define _SOCKS_H_

int  clientSocket_createConnection(void);

void clientSocket_setParams(char *serverIp, int serverPort);        // set connection params, e.g. "127.0.0.1" and 12345
int  clientSocket_write(unsigned char *bfr, int len);               // params: pointer to buffer, length of data to send. Returns length of sent data.
int  clientSocket_read(unsigned char *bfr, int len);                // params: pointer to buffer, maximum received length. Returns length of read data.

void serverSocket_setParams(int serverPort);                        // set connection params, e.g. 12345
int  serverSocket_write(unsigned char *bfr, int len);               // params: pointer to buffer, length of data to send. Returns length of sent data.
int  serverSocket_read(unsigned char *bfr, int len);                // params: pointer to buffer, maximum received length. Returns length of read data.


#endif

