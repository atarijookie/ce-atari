// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
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
#include "vbl.h"


// This file serves as middle layer between the client and client, as clients were compiled with Pure C cdecl calling convention,
// but this driver is compiled with gcc cdecl convention, and the parameters storage don't match, so this fixes the issue.
// Plus it can serve for Sting calls logging :)

int16  setvstr (char name[], char value[]);
char  *getvstr (char name[]);
void   house_keep(void);
int16  set_flag (int16 flag);
void   clear_flag (int16 flag);

uint32_t jumptable[40];

void initJumpTable(void)
{
    int i;

    for(i=0; i<40; i++) {
        jumptable[i] = 0;
    }

    i=0;
    jumptable[i++] = (uint32_t) KRmalloc_mid;
    jumptable[i++] = (uint32_t) KRfree_mid;
    jumptable[i++] = (uint32_t) KRgetfree_mid;
    jumptable[i++] = (uint32_t) KRrealloc_mid;
    jumptable[i++] = (uint32_t) get_error_text_mid;
    jumptable[i++] = (uint32_t) getvstr_mid;
    jumptable[i++] = (uint32_t) carrier_detect_mid;
    jumptable[i++] = (uint32_t) TCP_open_mid;
    jumptable[i++] = (uint32_t) TCP_close_mid;
    jumptable[i++] = (uint32_t) TCP_send_mid;
    jumptable[i++] = (uint32_t) TCP_wait_state_mid;
    jumptable[i++] = (uint32_t) TCP_ack_wait_mid;
    jumptable[i++] = (uint32_t) UDP_open_mid;
    jumptable[i++] = (uint32_t) UDP_close_mid;
    jumptable[i++] = (uint32_t) UDP_send_mid;
    jumptable[i++] = (uint32_t) CNkick_mid;
    jumptable[i++] = (uint32_t) CNbyte_count_mid;
    jumptable[i++] = (uint32_t) CNget_char_mid;
    jumptable[i++] = (uint32_t) CNget_NDB_mid;
    jumptable[i++] = (uint32_t) CNget_block_mid;
    jumptable[i++] = (uint32_t) housekeep_mid;
    jumptable[i++] = (uint32_t) resolve_mid;
    jumptable[i++] = (uint32_t) serial_dummy_mid;
    jumptable[i++] = (uint32_t) serial_dummy_mid;
    jumptable[i++] = (uint32_t) set_flag_mid;
    jumptable[i++] = (uint32_t) clear_flag_mid;
    jumptable[i++] = (uint32_t) CNgetinfo_mid;
    jumptable[i++] = (uint32_t) on_port_mid;
    jumptable[i++] = (uint32_t) off_port_mid;
    jumptable[i++] = (uint32_t) setvstr_mid;
    jumptable[i++] = (uint32_t) query_port_mid;
    jumptable[i++] = (uint32_t) CNgets_mid;
    jumptable[i++] = (uint32_t) ICMP_send_mid;
    jumptable[i++] = (uint32_t) ICMP_handler_mid;
    jumptable[i++] = (uint32_t) ICMP_discard_mid;
    jumptable[i++] = (uint32_t) TCP_info_mid;
    jumptable[i++] = (uint32_t) cntrl_port_mid;
}

void *KRmalloc_mid(uint8_t *sp)
{
    int32 size = getDwordFromSP();

    #ifdef DEBUG_STRING
    logStr("KRmalloc\n");
    #endif

    void *res = KRmalloc_internal(size);

    return res;
}

void KRfree_mid(uint8_t *sp)
{
    void *mem_block = getVoidPFromSP();

    #ifdef DEBUG_STRING
    logStr("KRfree\n");
    #endif

    KRfree_internal(mem_block);
}

int32 KRgetfree_mid(uint8_t *sp)
{
    int16 block_flag = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("KRgetfree\n");
    #endif

    int32 res;
    res =  KRgetfree_internal(block_flag);

    return res;
}

void *KRrealloc_mid(uint8_t *sp)
{
    void  *mem_block   = getVoidPFromSP();
    int32  new_size    = getDwordFromSP();

    #ifdef DEBUG_STRING
    logStr("KRrealloc\n");
    #endif

    void *res = KRrealloc_internal(mem_block, new_size);

    return res;
}

char *get_error_text_mid(uint8_t *sp)
{
    int16 error_code = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("get_error_text\n");
    #endif

    char *res = get_error_text(error_code);

    return res;
}

char *getvstr_mid(uint8_t *sp)
{
    char *name = getVoidPFromSP();

    #ifdef DEBUG_STRING
    logStr("getvstr\n");
    #endif

    char *res = getvstr(name);

    return res;
}

void serial_dummy_mid(uint8_t *sp)
{
    // Do really nothing - obsolete
}

int16 carrier_detect_mid(uint8_t *sp)
{
    // Do really nothing - obsolete
    return 1;
}

int16 TCP_open_mid(uint8_t *sp)
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

    vblEnabled = 0;
    int16 res = connection_open(1, rem_host, rem_port, tos, buff_size);
    vblEnabled = 1;

    #ifdef DEBUG_STRING
    showHexWord((uint16_t) res);
    logStr("\n");
    #endif

    return res;
}

int16 TCP_close_mid(uint8_t *sp)
{
    int16 handle      = getWordFromSP();
    int16 timeout     = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("TCP_close - handle: ");
    showHexByte(handle);
    logStr(", res: ");
    #endif

    vblEnabled = 0;
    int16 res = connection_close(1, handle, timeout);
    vblEnabled = 1;

    #ifdef DEBUG_STRING
    showHexWord((uint16_t) res);
    logStr("\n");
    #endif

    return res;
}

int16 TCP_send_mid(uint8_t *sp)
{
    int16 handle      = getWordFromSP();
    void *buffer      = getVoidPFromSP();
    int16 length      = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("TCP_send -- handle: ");
    showHexWord(handle);
    logStr(", buffer: ");
    showHexDword((uint32_t) buffer);
    logStr(", length: ");
    showHexWord(length);
    logStr("\n");
    #endif

    vblEnabled = 0;
    int16 res = TCP_send(handle, buffer, length);
    vblEnabled = 1;

    return res;
}

int16 TCP_wait_state_mid(uint8_t *sp)
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

    vblEnabled = 0;
    int16 res = TCP_wait_state(handle, wantedState, timeout);
    vblEnabled = 1;

    return res;
}

int16 TCP_ack_wait_mid(uint8_t *sp)
{
    int16 handle      = getWordFromSP();
    int16 timeout     = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("TCP_ack_wait\n");
    #endif

    vblEnabled = 0;
    int16 res = TCP_ack_wait(handle, timeout);
    vblEnabled = 1;

    return res;
}

int16 TCP_info_mid(uint8_t *sp)
{
    int16 handle    = getWordFromSP();
    TCPIB *tcp_info = getVoidPFromSP();

    #ifdef DEBUG_STRING
    logStr("TCP_info\n");
    #endif

    vblEnabled = 0;
    int16 res = TCP_info(handle, tcp_info);
    vblEnabled = 1;

    return res;
}

int16 UDP_open_mid (uint8_t *sp)
{
    uint32 rem_host    = getDwordFromSP();
    uint16 rem_port    = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("UDP_open\n");
    #endif

    vblEnabled = 0;
    int16 res = connection_open(0, rem_host, rem_port, 0, 0);
    vblEnabled = 1;

    return res;
}

int16 UDP_close_mid(uint8_t *sp)
{
    int16 handle = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("UDP_close\n");
    #endif

    vblEnabled = 0;
    int16 res = connection_close(0, handle, 0);
    vblEnabled = 1;

    return res;
}

int16 UDP_send_mid(uint8_t *sp)
{
    int16 handle = getWordFromSP();
    void *buffer = getVoidPFromSP();
    int16 length = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("UDP_send\n");
    #endif

    vblEnabled = 0;
    int16 res = connection_send(0, handle, buffer, length);
    vblEnabled = 1;

    return res;
}

int16 CNkick_mid(uint8_t *sp)
{
    int16 handle = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("CNkick\n");
    #endif

    vblEnabled = 0;
    int16 res = CNkick(handle);
    vblEnabled = 1;

    return res;
}

int16 CNbyte_count_mid(uint8_t *sp)
{
    int16 handle = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("CNbyte_count - handle: ");
    showHexByte((uint8_t) handle);
    logStr(", res: ");
    #endif

    vblEnabled = 0;
    int16 res = CNbyte_count(handle);
    vblEnabled = 1;

    #ifdef DEBUG_STRING
    showHexWord((uint32_t) res);
    logStr("\n");
    #endif

    return res;
}

int16 CNget_char_mid(uint8_t *sp)
{
    int16 handle = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("CNget_char: ");
    #endif

    vblEnabled = 0;
    int16 res = CNget_char(handle);
    vblEnabled = 1;

    #ifdef DEBUG_STRING
    showHexWord((uint32_t) res);

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

NDB *CNget_NDB_mid(uint8_t *sp)
{
    int16 handle = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("CNget_NDB\n");
    #endif

    vblEnabled = 0;
    NDB *res = CNget_NDB (handle);
    vblEnabled = 1;

    return res;
}

int16 CNget_block_mid(uint8_t *sp)
{
    int16 handle  = getWordFromSP();
    void *buffer  = getVoidPFromSP();
    int16 length  = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("CNget_block - handle: ");
    showHexByte(handle);
    logStr(", buffer: ");
    showHexDword((uint32_t) buffer);
    logStr(", length: ");
    showHexWord(length);
    logStr(", res: ");
    #endif

    vblEnabled = 0;
    int16 res = CNget_block(handle, buffer, length);
    vblEnabled = 1;

    #ifdef DEBUG_STRING
    showHexWord(res);
    logStr("\n");

    // show last 4 bytes
    uint8_t *p = (uint8_t *) buffer;

    logStr("last 4 bytes: ");
    if(res >= 4) { showHexByte(p[res - 4]); logStr(" "); }
    if(res >= 3) { showHexByte(p[res - 3]); logStr(" "); }
    if(res >= 2) { showHexByte(p[res - 2]); logStr(" "); }
    if(res >= 1) { showHexByte(p[res - 1]); logStr(" "); }
    logStr("\n");
    #endif

    return res;
}

void housekeep_mid(uint8_t *sp)
{
    house_keep();
}

int16 resolve_mid(uint8_t *sp)
{
    char *   domain      = getVoidPFromSP();
    char **  real_domain = getVoidPFromSP();
    uint32 * ip_list     = getVoidPFromSP();
    int16    ip_num      = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("resolve\n");
    #endif

    vblEnabled = 0;
    int16 res = resolve(domain, real_domain, ip_list, ip_num);
    vblEnabled = 1;

    return res;
}

int16 set_flag_mid(uint8_t *sp)
{
    int16 flag = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("set_flag\n");
    #endif

    int16 res = set_flag(flag);

    return res;
}

void clear_flag_mid(uint8_t *sp)
{
    int16 flag = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("clear_flag\n");
    #endif

    clear_flag(flag);
}

CIB *CNgetinfo_mid(uint8_t *sp)
{
    int16 handle = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("CNgetinfo - handle: ");
    showHexWord(handle);
    logStr(", res: ");
    #endif

    vblEnabled = 0;
    CIB *res = CNgetinfo(handle);
    vblEnabled = 1;

    #ifdef DEBUG_STRING
    showHexDword((uint32_t) res);
    logStr("\n");
    #endif

    return res;
}

int16 setvstr_mid(uint8_t *sp)
{
    char *name    = getVoidPFromSP();
    char *value   = getVoidPFromSP();

    #ifdef DEBUG_STRING
    logStr("setvstr\n");
    #endif

    int16 res = setvstr(name, value);

    return res;
}

int16 CNgets_mid(uint8_t *sp)
{
    int16  handle      = getWordFromSP();
    char * buffer      = getVoidPFromSP();
    int16  length      = getWordFromSP();
    char   delimiter   = getByteFromSP();

    #ifdef DEBUG_STRING
    logStr("CNgets\n");
    #endif

    vblEnabled = 0;
    int16 res = CNgets(handle, buffer, length, delimiter);
    vblEnabled = 1;

    return res;
}

int16 ICMP_send_mid(uint8_t *sp)
{
    uint32  dest        = getDwordFromSP();
    uint8   type        = getByteFromSP();
    uint8   code        = getByteFromSP();
    void   *data        = getVoidPFromSP();
    uint16  dat_length  = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("ICMP_send\n");
    #endif

    vblEnabled = 0;
    int16 res = ICMP_send(dest, type, code, data, dat_length);
    vblEnabled = 1;

    return res;
}

int16 ICMP_handler_mid(uint8_t *sp)
{
    int16 (*handler)(IP_DGRAM *)    = getVoidPFromSP();
    int16 flag                      = getWordFromSP();

    #ifdef DEBUG_STRING
    logStr("ICMP_handler\n");
    #endif

    vblEnabled = 0;
    int16 res = ICMP_handler (handler, flag);
    vblEnabled = 1;

    return res;
}

void ICMP_discard_mid(uint8_t *sp)
{
    IP_DGRAM *dgram = getVoidPFromSP();

    #ifdef DEBUG_STRING
    logStr("ICMP_discard\n");
    #endif

    vblEnabled = 0;
    ICMP_discard(dgram);
    vblEnabled = 1;
}

int16 on_port_mid(uint8_t *sp)
{
    char *port_name = getVoidPFromSP();

    #ifdef DEBUG_STRING
    logStr("on_port\n");
    #endif

    int16 res = on_port(port_name);

    return res;
}

void off_port_mid(uint8_t *sp)
{
    char *port_name = getVoidPFromSP();

    #ifdef DEBUG_STRING
    logStr("off_port\n");
    #endif

    off_port(port_name);
}

int16 query_port_mid(uint8_t *sp)
{
    char *port_name = getVoidPFromSP();

    #ifdef DEBUG_STRING
    logStr("query_port\n");
    #endif

    int16 res = query_port(port_name);

    return res;
}

int16 cntrl_port_mid(uint8_t *sp)
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

