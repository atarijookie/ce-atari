#ifndef _UDP_H_
#define _UDP_H_

int16 UDP_open  (uint32 rem_host, uint16 rem_port);
int16 UDP_close (int16 handle);
int16 UDP_send  (int16 handle, void *buffer, int16 length);

#endif
