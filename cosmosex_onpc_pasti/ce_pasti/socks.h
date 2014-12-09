#ifndef _SOCKS_H_
#define _SOCKS_H_

int  clientSocket_createConnection(void);
int  clientSocket_write(unsigned char *bfr, int len);               // params: pointer to buffer, length of data to send. Returns length of sent data.
int  clientSocket_read(unsigned char *bfr, int len);                // params: pointer to buffer, maximum received length. Returns length of read data.

#endif

