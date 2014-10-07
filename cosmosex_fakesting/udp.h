#ifndef _UDP_H_
#define _UDP_H_

int16      /* cdecl */  UDP_open (uint32 rem_host, uint16 rem_port);
int16      /* cdecl */  UDP_close (int16 connec);
int16      /* cdecl */  UDP_send (int16 connec, void *buffer, int16 length);

#endif
