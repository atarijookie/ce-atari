#ifndef _CON_MAN_H_
#define _CON_MAN_H_

//-------------------------------------
// connection info function
int16 CNkick        (int16 handle);
CIB * CNgetinfo     (int16 handle);
int16 CNbyte_count  (int16 handle);
//-------------------------------------
// data retrieval functions
NDB * CNget_NDB     (int16 handle);
int16 CNget_block   (int16 handle, void *buffer, int16 length);
int16 CNgets        (int16 handle, char *buffer, int16 length, char delimiter);
int16 CNget_char    (int16 handle);

//-------------------------------------
// other functions

int16 resolve (char *domain, char **real_domain, uint32 *ip_list, int16 ip_num);

//-------------------------------------
// helper functions
void init_con_info(void);
void update_con_info(void);

void structs_init(void);
int  handle_valid(int16 h);

int16 connection_open   (int tcpNotUdp, uint32 rem_host, uint16 rem_port, uint16 tos, uint16 buff_size);
int16 connection_close  (int tcpNotUdp, int16 handle, int16 mode, int16 *result);
int16 connection_send   (int tcpNotUdp, int16 handle, void *buffer, int16 length);

BYTE  fillReadBuffer(int16 handle);
DWORD readData(int16 handle, BYTE *bfr, DWORD cnt, BYTE seekOffset);
DWORD read_small(int16 handle, DWORD countNeeded, BYTE *buffer);
DWORD read_big(int16 handle, DWORD countNeeded, BYTE *buffer);
//-------------------------------------

#endif