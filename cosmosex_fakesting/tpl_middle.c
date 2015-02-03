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

DWORD jumptable[40];

void initJumpTable(void)
{
    int i;
    
    for(i=0; i<40; i++) {
        jumptable[i] = 0;
    }
    
    i=0;
    jumptable[i++] = (DWORD) KRmalloc_mid;
    jumptable[i++] = (DWORD) KRfree_mid;
    jumptable[i++] = (DWORD) KRgetfree_mid;
    jumptable[i++] = (DWORD) KRrealloc_mid;
    jumptable[i++] = (DWORD) get_error_text_mid;
    jumptable[i++] = (DWORD) getvstr_mid;
    jumptable[i++] = (DWORD) carrier_detect_mid;
    jumptable[i++] = (DWORD) TCP_open_mid;
    jumptable[i++] = (DWORD) TCP_close_mid;
    jumptable[i++] = (DWORD) TCP_send_mid;
    jumptable[i++] = (DWORD) TCP_wait_state_mid;
    jumptable[i++] = (DWORD) TCP_ack_wait_mid;
    jumptable[i++] = (DWORD) UDP_open_mid;
    jumptable[i++] = (DWORD) UDP_close_mid;
    jumptable[i++] = (DWORD) UDP_send_mid;
    jumptable[i++] = (DWORD) CNkick_mid;
    jumptable[i++] = (DWORD) CNbyte_count_mid;
    jumptable[i++] = (DWORD) CNget_char_mid;
    jumptable[i++] = (DWORD) CNget_NDB_mid;
    jumptable[i++] = (DWORD) CNget_block_mid;
    jumptable[i++] = (DWORD) housekeep_mid;
    jumptable[i++] = (DWORD) resolve_mid;
    jumptable[i++] = (DWORD) serial_dummy_mid;
    jumptable[i++] = (DWORD) serial_dummy_mid;
    jumptable[i++] = (DWORD) set_flag_mid;
    jumptable[i++] = (DWORD) clear_flag_mid;
    jumptable[i++] = (DWORD) CNgetinfo_mid;
    jumptable[i++] = (DWORD) on_port_mid;
    jumptable[i++] = (DWORD) off_port_mid;
    jumptable[i++] = (DWORD) setvstr_mid;
    jumptable[i++] = (DWORD) query_port_mid;
    jumptable[i++] = (DWORD) CNgets_mid;
    jumptable[i++] = (DWORD) ICMP_send_mid;
    jumptable[i++] = (DWORD) ICMP_handler_mid;
    jumptable[i++] = (DWORD) ICMP_discard_mid;
    jumptable[i++] = (DWORD) TCP_info_mid;
    jumptable[i++] = (DWORD) cntrl_port_mid;
}

void *KRmalloc_mid(BYTE *sp)
{
    int32 size = getDwordFromSP();

    #ifdef DEBUG_STRING
    logStr("KRmalloc\n");
    #endif

    void *res = KRmalloc_internal(size);
    
    return res;
}

void KRfree_mid(BYTE *sp)
{
    void *mem_block = getVoidPFromSP();
    
    #ifdef DEBUG_STRING
    logStr("KRfree\n");
    #endif

    KRfree_internal(mem_block);
}

int32 KRgetfree_mid(BYTE *sp)
{
    int16 block_flag = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("KRgetfree\n");
    #endif

    int32 res;
    res =  KRgetfree_internal(block_flag);

    return res;
}

void *KRrealloc_mid(BYTE *sp)
{
    void  *mem_block   = getVoidPFromSP();
    int32  new_size    = getDwordFromSP();

    #ifdef DEBUG_STRING
    logStr("KRrealloc\n");
    #endif
    
    void *res = KRrealloc_internal(mem_block, new_size);
    
    return res;
}
    
char *get_error_text_mid(BYTE *sp)
{
    int16 error_code = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("get_error_text\n");
    #endif

    char *res = get_error_text(error_code);
    
    return res;
}

char *getvstr_mid(BYTE *sp)
{
    char *name = getVoidPFromSP();

    #ifdef DEBUG_STRING
    logStr("getvstr\n");
    #endif
    
    char *res = getvstr(name);

    return res;
}

void serial_dummy_mid(BYTE *sp)
{
    // Do really nothing - obsolete
}

int16 carrier_detect_mid(BYTE *sp)
{
    // Do really nothing - obsolete
    return 1;
} 

int16 TCP_open_mid(BYTE *sp)
{
    uint32 rem_host     = getDwordFromSP();
    uint16 rem_port     = getWordFromSP();
    uint16 tos          = getWordFromSP();
    uint16 buff_size    = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("TCP_open - rem_host: ");
    showHexDword(rem_host);
    logStr(", rem_port: ");
    showHexWord(rem_port);
    logStr(", res: ");
    #endif

    int16 res = connection_open(1, rem_host, rem_port, tos, buff_size);
//    int16 res = 0;
    
    #ifdef DEBUG_STRING
    showHexWord((WORD) res);
    logStr("\n");
    #endif
    
    return res;
}

int16 TCP_close_mid(BYTE *sp)
{
    int16 handle      = getWordFromSP();
    int16 timeout     = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("TCP_close - handle: ");
    showHexByte(handle);
    logStr(", res: ");
    #endif

    int16 res = connection_close(1, handle, timeout);

    #ifdef DEBUG_STRING
    showHexWord((WORD) res);
    logStr("\n");
    #endif
    
    return res;
}

int16 TCP_send_mid(BYTE *sp)
{
    int16 handle      = getWordFromSP();
    void *buffer      = getVoidPFromSP();
    int16 length      = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("TCP_send -- handle: ");
    showHexWord(handle);
    logStr(", buffer: ");
    showHexDword((DWORD) buffer);
    logStr(", length: ");
    showHexWord(length);
    logStr("\n");
    #endif
    
    int16 res = TCP_send(handle, buffer, length);
    return res;
}
    
int16 TCP_wait_state_mid(BYTE *sp)
{
    int16 handle      = getWordFromSP();
    int16 wantedState = getWordFromSP();
    int16 timeout     = getWordFromSP();

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
    
int16 TCP_ack_wait_mid(BYTE *sp)
{
    int16 handle      = getWordFromSP();
    int16 timeout     = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("TCP_ack_wait\n");
    #endif

    int16 res = TCP_ack_wait(handle, timeout);

    return res;
}
    
int16 TCP_info_mid(BYTE *sp)
{
    int16 handle    = getWordFromSP();
    TCPIB *tcp_info = getVoidPFromSP();

    #ifdef DEBUG_STRING
    logStr("TCP_info\n");
    #endif
    
    int16 res = TCP_info(handle, tcp_info);

    return res;
}    

int16 UDP_open_mid (BYTE *sp)
{
    uint32 rem_host    = getDwordFromSP();
    uint16 rem_port    = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("UDP_open\n");
    #endif

    int16 res = connection_open(0, rem_host, rem_port, 0, 0);
    
    return res;
}

int16 UDP_close_mid(BYTE *sp)
{
    int16 handle = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("UDP_close\n");
    #endif
    
    int16 res = connection_close(0, handle, 0);
    
    return res;
}

int16 UDP_send_mid(BYTE *sp)
{
    int16 handle = getWordFromSP();
    void *buffer = getVoidPFromSP();
    int16 length = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("UDP_send\n");
    #endif
    
    int16 res = connection_send(0, handle, buffer, length);
    
    return res;
}
    
int16 CNkick_mid(BYTE *sp)
{
    int16 handle = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("CNkick\n");
    #endif
    
    int16 res = CNkick(handle);

    return res;
}

int16 CNbyte_count_mid(BYTE *sp)
{
    int16 handle = getWordFromSP();

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

int16 CNget_char_mid(BYTE *sp)
{   
    int16 handle = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("CNget_char: ");
    #endif

    int16 res = CNget_char(handle);

    #ifdef DEBUG_STRING
    showHexWord((DWORD) res);
    
    if(res >= 0 && res <= 255) {
        char tmp[2];
        
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

NDB *CNget_NDB_mid(BYTE *sp)
{
    int16 handle = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("CNget_NDB\n");
    #endif
    
    NDB *res = CNget_NDB (handle);

    return res;
}    

int16 CNget_block_mid(BYTE *sp)
{
    int16 handle  = getWordFromSP();
    void *buffer  = getVoidPFromSP();
    int16 length  = getWordFromSP();

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

    logStr("last 4 bytes: ");
    if(res >= 4) { showHexByte(p[res - 4]); logStr(" "); }
    if(res >= 3) { showHexByte(p[res - 3]); logStr(" "); }
    if(res >= 2) { showHexByte(p[res - 2]); logStr(" "); }
    if(res >= 1) { showHexByte(p[res - 1]); logStr(" "); }
    logStr("\n");
    #endif

    return res;
}

void housekeep_mid(BYTE *sp)
{
    house_keep();
}

int16 resolve_mid(BYTE *sp)
{
    char *   domain      = getVoidPFromSP();
    char **  real_domain = getVoidPFromSP();
    uint32 * ip_list     = getVoidPFromSP();
    int16    ip_num      = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("resolve\n");
    #endif
    
    int16 res = resolve(domain, real_domain, ip_list, ip_num);
    return res;
}
    
int16 set_flag_mid(BYTE *sp)
{
    int16 flag = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("set_flag\n");
    #endif
    
    int16 res = set_flag(flag);

    return res;
}    

void clear_flag_mid(BYTE *sp)
{
    int16 flag = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("clear_flag\n");
    #endif

    clear_flag(flag);
}    
    
CIB *CNgetinfo_mid(BYTE *sp)
{
    int16 handle = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("CNgetinfo - handle: ");
    showHexWord(handle);
    logStr(", res: ");
    #endif

    CIB *res = CNgetinfo(handle);

    #ifdef DEBUG_STRING
    showHexDword((DWORD) res);
    logStr("\n");
    #endif

    return res;
}
    
int16 on_port_mid(BYTE *sp)
{
    char *port_name = getVoidPFromSP();

    #ifdef DEBUG_STRING
    logStr("on_port\n");
    #endif
    
    int16 res = on_port(port_name);
    
    return res;
}

void off_port_mid(BYTE *sp)
{
    char *port_name = getVoidPFromSP();

    #ifdef DEBUG_STRING
    logStr("off_port\n");
    #endif

    off_port(port_name);
}
 
int16 setvstr_mid(BYTE *sp)
{
    char *name    = getVoidPFromSP();
    char *value   = getVoidPFromSP();

    #ifdef DEBUG_STRING
    logStr("setvstr\n");
    #endif
    
    int16 res = setvstr(name, value);

    return res;
}

int16 query_port_mid(BYTE *sp)
{
    char *port_name = getVoidPFromSP();

    #ifdef DEBUG_STRING
    logStr("query_port\n");
    #endif
    
    int16 res = query_port(port_name);

    return res;
}

int16 CNgets_mid(BYTE *sp)
{
    int16  handle      = getWordFromSP();
    char * buffer      = getVoidPFromSP();
    int16  length      = getWordFromSP();
    char   delimiter   = getByteFromSP();

    #ifdef DEBUG_STRING
    logStr("CNgets\n");
    #endif

    int16 res = CNgets(handle, buffer, length, delimiter);

    return res;
}

int16 ICMP_send_mid(BYTE *sp)
{
    uint32  dest        = getDwordFromSP();
    uint8   type        = getByteFromSP();
    uint8   code        = getByteFromSP();
    void   *data        = getVoidPFromSP();
    uint16  dat_length  = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("ICMP_send\n");
    #endif
    
    int16 res = ICMP_send(dest, type, code, data, dat_length);

    return res;
}

int16 ICMP_handler_mid(BYTE *sp)
{
    int16 (*handler)(IP_DGRAM *)    = getVoidPFromSP();
    int16 flag                      = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("ICMP_handler\n");
    #endif
    
    int16 res = ICMP_handler (handler, flag);

    return res;
}

void ICMP_discard_mid(BYTE *sp)
{
    IP_DGRAM *dgram = getVoidPFromSP();

    #ifdef DEBUG_STRING
    logStr("ICMP_discard\n");
    #endif

    ICMP_discard(dgram);
}

int16 cntrl_port_mid(BYTE *sp)
{
    char   *port_name   = getVoidPFromSP();
    uint32  argument    = getDwordFromSP();
    int16   code        = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("cntrl_port\n");
    #endif

    int16 res = cntrl_port(port_name, argument, code);

    return res;
}

