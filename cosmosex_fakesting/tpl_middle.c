#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <support.h>
#include <stdint.h>

#include "globdefs.h"
#include "stdlib.h"
#include "tpl_middle.h"
#include "setup.h"
#include "tcp.h"
#include "udp.h"
#include "icmp.h"
#include "con_man.h"
#include "port.h"


// This file serves as middle layer between the client and client, as clients were compiled with Pure C cdecl calling convention,
// but this driver is compiled with gcc cdecl convention, and the parameters storage don't match, so this fixes the issue.
// Plus it can serve for Sting calls logging :)

int16  setvstr (char name[], char value[]);
char  *getvstr (char name[]);
void   house_keep(void);
int16  set_flag (int16 flag);
void   clear_flag (int16 flag);

//#define DEBUG_STRING

void *KRmalloc_mid(int32 size)
{
    getStackPointer();
    size = getDwordFromSP();

    #ifdef DEBUG_STRING
    logStr("KRmalloc\n");
    #endif

    void *res = KRmalloc_internal(size);
    
    return res;
}

void KRfree_mid(void *mem_block)
{
    getStackPointer();
    mem_block = getVoidPFromSP();
    
    #ifdef DEBUG_STRING
    logStr("KRfree\n");
    #endif

    KRfree_internal(mem_block);
}

int32 KRgetfree_mid (int16 block_flag)
{
    getStackPointer();
    block_flag = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("KRgetfree\n");
    #endif

    int32 res;
    res =  KRgetfree_internal(block_flag);

    return res;
}

void *KRrealloc_mid (void *mem_block, int32 new_size)
{
    getStackPointer();
    mem_block   = getVoidPFromSP();
    new_size    = getDwordFromSP();

    #ifdef DEBUG_STRING
    logStr("KRrealloc\n");
    #endif
    
    void *res =  KRrealloc_internal(mem_block, new_size);
    
    return res;
}
    
char *get_error_text_mid(int16 error_code)
{
    getStackPointer();
    error_code = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("get_error_text\n");
    #endif

    char *res = get_error_text(error_code);
    
    return res;
}

char *getvstr_mid(char *name)
{
    getStackPointer();
    name = getVoidPFromSP();

    #ifdef DEBUG_STRING
    logStr("getvstr\n");
    #endif
    
    char *res = getvstr(name);

    return res;
}

void serial_dummy_mid(void)
{
    // Do really nothing - obsolete
}

int16 carrier_detect_mid(void)
{
    // Do really nothing - obsolete
    return 1;
} 

int16 TCP_open_mid(uint32 rem_host, uint16 rem_port, uint16 tos, uint16 buff_size)
{
    getStackPointer();
    rem_host    = getDwordFromSP();
    rem_port    = getWordFromSP();
    tos         = getWordFromSP();
    buff_size   = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("TCP_open - res: ");
    #endif

    int16 res = TCP_open(rem_host, rem_port, tos, buff_size);
    
    #ifdef DEBUG_STRING
    showHexWord((WORD) res);
    logStr("\n");
    #endif
    
    return res;
}

int16 TCP_close_mid(int16 handle, int16 timeout)
{
    getStackPointer();
    handle      = getWordFromSP();
    timeout     = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("TCP_close - handle: ");
    showHexByte(handle);
    logStr(", res: ");
    #endif

    int16 res = TCP_close(handle, timeout);

    #ifdef DEBUG_STRING
    showHexWord((WORD) res);
    logStr("\n");
    #endif
    
    return res;
}

int16 TCP_send_mid(int16 handle, void *buffer, int16 length)
{
    getStackPointer();
    handle      = getWordFromSP();
    buffer      = getVoidPFromSP();
    length      = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("TCP_send\n");
    #endif
    
    int16 res = TCP_send(handle, buffer, length);
    
    return res;
}
    
int16 TCP_wait_state_mid(int16 handle, int16 wantedState, int16 timeout)
{
    getStackPointer();
    handle      = getWordFromSP();
    wantedState = getWordFromSP();
    timeout     = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("TCP_wait_state - wantedState: ");
    showHexByte(wantedState);
    logStr(", timeout: ");
    showHexByte(timeout);
    logStr("\n");
    #endif
    
    int16 res = TCP_wait_state(handle, wantedState, timeout);
    
    return res;
}
    
int16 TCP_ack_wait_mid(int16 handle, int16 timeout)
{
    getStackPointer();
    handle      = getWordFromSP();
    timeout     = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("TCP_ack_wait\n");
    #endif

    int16 res = TCP_ack_wait(handle, timeout);

    return res;
}
    
int16 TCP_info_mid(int16 handle, TCPIB *tcp_info)
{
    getStackPointer();
    handle      = getWordFromSP();
    tcp_info    = getVoidPFromSP();

    #ifdef DEBUG_STRING
    logStr("TCP_info\n");
    #endif
    
    int16 res = TCP_info(handle, tcp_info);

    return res;
}    

int16 UDP_open_mid (uint32 rem_host, uint16 rem_port)
{
    getStackPointer();
    rem_host    = getDwordFromSP();
    rem_port    = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("UDP_open\n");
    #endif

    int16 res = UDP_open(rem_host, rem_port);
    
    return res;
}

int16 UDP_close_mid(int16 handle)
{
    getStackPointer();
    handle      = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("UDP_close\n");
    #endif
    
    int16 res = UDP_close(handle);
    
    return res;
}

int16 UDP_send_mid(int16 handle, void *buffer, int16 length)
{
    getStackPointer();
    handle      = getWordFromSP();
    buffer      = getVoidPFromSP();
    length      = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("UDP_send\n");
    #endif
    
    int16 res = UDP_send(handle, buffer, length);
    
    return res;
}
    
int16 CNkick_mid(int16 handle)
{
    getStackPointer();
    handle = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("CNkick\n");
    #endif
    
    int16 res = CNkick(handle);

    return res;
}
    
int16 CNbyte_count_mid(int16 handle)
{
    getStackPointer();
    handle = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("CNbyte_count - handle: ");
    showHexByte((BYTE) handle);
    logStr(", res: ");
    #endif

    int16 res = CNbyte_count(handle);

    #ifdef DEBUG_STRING
    showHexWord((DWORD) res);
    logStr("\n");
    #endif
    
    return res;
}

int16 CNget_char_mid(int16 handle)
{   
    char tmp[2];

    getStackPointer();
    handle = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("CNget_char: ");
    #endif
    
    int16 res = CNget_char(handle);

    #ifdef DEBUG_STRING
    showHexWord((DWORD) res);
    
    if(res >= 0 && res <= 255) {
        logStr(" '");
        tmp[0] = (char) res;
        tmp[1] = 0;
        logStr(tmp);
        logStr("'");
    }
    
    logStr("\n");
    #endif

    return res;
}

NDB *CNget_NDB_mid(int16 handle)
{
    getStackPointer();
    handle = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("CNget_NDB\n");
    #endif
    
    NDB *res = CNget_NDB (handle);

    return res;
}    

int16 CNget_block_mid(int16 handle, void *buffer, int16 length)
{
    getStackPointer();
    handle  = getWordFromSP();
    buffer  = getVoidPFromSP();
    length  = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("CNget_block - handle: ");
    showHexByte(handle);
    logStr(", buffer: ");
    showHexDword((DWORD) buffer);
    logStr(", length: ");
    showHexWord(length);
    logStr(", res: ");
    #endif

    int16 res = CNget_block(handle, buffer, length);

    #ifdef DEBUG_STRING
    showHexWord(res);
    logStr("\n");

    // show last 4 bytes
    BYTE *p = (BYTE *) buffer;

    if(res >= 4) { showHexByte(p[res - 4]); Cconout(' '); }
    if(res >= 3) { showHexByte(p[res - 3]); Cconout(' '); }
    if(res >= 2) { showHexByte(p[res - 2]); Cconout(' '); }
    if(res >= 1) { showHexByte(p[res - 1]); Cconout(' '); }
    logStr("\n");
    #endif

    return res;
}

void house_keep_mid(void)
{
    house_keep();
}

int16 resolve_mid(char *domain, char **real_domain, uint32 *ip_list, int16 ip_num)
{
    getStackPointer();
    domain      = getVoidPFromSP();
    real_domain = getVoidPFromSP();
    ip_list     = getVoidPFromSP();
    ip_num      = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("resolve\n");
    #endif
    
    int16 res = resolve (domain, real_domain, ip_list, ip_num);

    return res;
}
    
int16 set_flag_mid(int16 flag)
{
    getStackPointer();
    flag = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("set_flag\n");
    #endif
    
    int16 res = set_flag(flag);

    return res;
}    

void clear_flag_mid(int16 flag)
{
    getStackPointer();
    flag = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("clear_flag\n");
    #endif

    clear_flag(flag);
}    
    
CIB *CNgetinfo_mid(int16 handle)
{
    getStackPointer();
    handle = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("CNgetinfo - res: ");
    #endif

    CIB *res = CNgetinfo(handle);

    #ifdef DEBUG_STRING
    showHexDword((DWORD) res);
    logStr("\n");
    #endif

    return res;
}
    
int16 on_port_mid(char *port_name)
{
    getStackPointer();
    port_name = getVoidPFromSP();

    #ifdef DEBUG_STRING
    logStr("on_port\n");
    #endif
    
    int16 res = on_port(port_name);
    
    return res;
}

void off_port_mid(char *port_name)
{
    getStackPointer();
    port_name = getVoidPFromSP();

    #ifdef DEBUG_STRING
    logStr("off_port\n");
    #endif

    off_port(port_name);
}
 
int16 setvstr_mid(char *name, char *value)
{
    getStackPointer();
    name    = getVoidPFromSP();
    value   = getVoidPFromSP();

    #ifdef DEBUG_STRING
    logStr("setvstr\n");
    #endif
    
    int16 res = setvstr(name, value);

    return res;
}

int16 query_port_mid(char *port_name)
{
    getStackPointer();
    port_name = getVoidPFromSP();

    #ifdef DEBUG_STRING
    logStr("query_port\n");
    #endif
    
    int16 res = query_port(port_name);

    return res;
}

int16 CNgets_mid(int16 handle, char *buffer, int16 length, char delimiter)
{
    getStackPointer();
    handle      = getWordFromSP();
    buffer      = getVoidPFromSP();
    length      = getWordFromSP();
    delimiter   = getByteFromSP();

    #ifdef DEBUG_STRING
    logStr("CNgets\n");
    #endif

    int16 res = CNgets(handle, buffer, length, delimiter);

    return res;
}

int16 ICMP_send_mid(uint32 dest, uint8 type, uint8 code, void *data, uint16 dat_length)
{
    getStackPointer();
    dest        = getDwordFromSP();
    type        = getByteFromSP();
    code        = getByteFromSP();
    data        = getVoidPFromSP();
    dat_length  = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("ICMP_send\n");
    #endif
    
    int16 res = ICMP_send(dest, type, code, data, dat_length);

    return res;
}

int16 ICMP_handler_mid(int16 (* handler) (IP_DGRAM *), int16 flag)
{
    getStackPointer();
    handler = getVoidPFromSP();
    flag    = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("ICMP_handler\n");
    #endif
    
    int16 res = ICMP_handler (handler, flag);

    return res;
}

void ICMP_discard_mid(IP_DGRAM *dgram)
{
    getStackPointer();
    dgram = getVoidPFromSP();

    #ifdef DEBUG_STRING
    logStr("ICMP_discard\n");
    #endif

    ICMP_discard(dgram);
}

int16 cntrl_port_mid(char *port_name, uint32 argument, int16 code)
{
    getStackPointer();
    port_name   = getVoidPFromSP();
    argument    = getDwordFromSP();
    code        = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("cntrl_port\n");
    #endif

    int16 res = cntrl_port(port_name, argument, code);

    return res;
}

int16 UDP_info_mid(int16 handle, UDPIB *pUdpIb)
{
    #ifdef DEBUG_STRING
    logStr("UDP_info - not implemented\n");
    #endif

    return E_FNAVAIL;
}

int16 RAW_open_mid(uint32 param)
{
    #ifdef DEBUG_STRING
    logStr("RAW_open - not implemented\n");
    #endif

    return E_FNAVAIL;
}

int16 RAW_close_mid(int16 handle)
{
    #ifdef DEBUG_STRING
    logStr("RAW_close - not implemented\n");
    #endif

    return E_FNAVAIL;
}

int16 RAW_out_mid(int16 handle, void *pAParam, int16 bParam, uint32 cParam)
{
    #ifdef DEBUG_STRING
    logStr("RAW_out - not implemented\n");
    #endif
    
    return E_FNAVAIL;
}

int16 CN_setopt_mid(int16 handle, int16 aParam, const void *pBParam, int16 cParam)
{
    #ifdef DEBUG_STRING
    logStr("CN_setopt - not implemented\n");
    #endif
    
    return E_FNAVAIL;
}

int16 CN_getopt_mid(int16 handle, int16 aParam, void *pBParam, int16 *pCParam)
{
    #ifdef DEBUG_STRING
    logStr("CN_getopt - not implemented\n");
    #endif
    
    return E_FNAVAIL;
}

void CNfree_NDB_mid(int16 aParam, NDB *pNdb)
{
    #ifdef DEBUG_STRING
    logStr("CNfree_NDB - not implemented\n");
    #endif
}
    

