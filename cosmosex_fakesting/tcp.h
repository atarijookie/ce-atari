#ifndef _TCP_H_
#define _TCP_H_

int16      /* cdecl */  TCP_open (uint32 rem_host, uint16 rem_port, uint16 tos, uint16 size);
int16      /* cdecl */  TCP_close (int16 connec, int16 mode, int16 *result);
int16      /* cdecl */  TCP_send (int16 connec, void *buffer, int16 length);
int16      /* cdecl */  TCP_wait_state (int16 connec, int16 state, int16 timeout);
int16      /* cdecl */  TCP_ack_wait (int16 connec, int16 timeout);
int16      /* cdecl */  TCP_info (int16 connec, void *tcp_info);

#endif

