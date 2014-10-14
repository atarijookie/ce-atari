#ifndef _TCP_H_
#define _TCP_H_

int16 TCP_open          (uint32 rem_host, uint16 rem_port, uint16 tos, uint16 size);
int16 TCP_close         (int16 handle, int16 mode, int16 *result);
int16 TCP_send          (int16 handle, void *buffer, int16 length);

int16 TCP_wait_state    (int16 handle, int16 state, int16 timeout);
int16 TCP_ack_wait      (int16 handle, int16 timeout);
int16 TCP_info          (int16 handle, TCPIB *tcp_info);

#endif

