#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <support.h>
#include <stdint.h>
#include <stdio.h>

#include "globdefs.h"
#include "hdd_if.h"
#include "con_man.h"
#include "icmp.h"
#include "setup.h"

//---------------------
// ACSI / CosmosEx stuff
#include "acsi.h"
#include "ce_commands.h"
#include "stdlib.h"

//--------------------------------------

extern  uint32  localIP;
extern  BYTE    FastRAMBuffer[]; 

TConInfo conInfo[MAX_HANDLE];                                                // this holds info about each connection

static void connection_send_block(BYTE netCmd, BYTE handle, WORD length, BYTE *buffer);
//--------------------------------------
#define NO_CHANGE_W     0xfffe
#define NO_CHANGE_DW    0xfffffffe

void setCIB(BYTE *cib, WORD protocol, WORD lPort, WORD rPort, DWORD rHost, DWORD lHost, WORD status);

#define CIB_PROTO   0
#define CIB_LPORT   1
#define CIB_RPORT   2
#define CIB_RHOST   3
#define CIB_LHOST   4
#define CIB_STATUS  5

DWORD getCIBitem(BYTE *cib, BYTE item);
//--------------------------------------
// connection info function

int16 CNkick(int16 handle)
{
    update_con_info(FALSE);             // update connections info structs (max once per 100 ms)

    if(!handle_valid(handle)) {         // we don't have this handle? fail
        return E_BADHANDLE;
    }

	return E_NORMAL;
}

CIB *CNgetinfo(int16 handle)
{
    // Note: do return valid CIB* even for closed handles, the client app might expect this to be not NULL even if the connection is opening / closing / whatever

    if(handle < 0 || handle >= MAX_HANDLE) {        // handle out of range? fail
        return (CIB *) NULL;
    }

    update_con_info(FALSE);                         // update connections info structs (max once per 100 ms)

	return &conInfo[handle].cib;                    // return pointer to correct CIB
}

int16 CNbyte_count (int16 handle)
{
    if(!handle_valid(handle)) {                     // we don't have this handle? fail
        return E_BADHANDLE;
    }
    
    update_con_info(FALSE);                         // update connections info structs (max once per 100 ms)
    TConInfo *ci = &conInfo[handle];                // we're working with this connection
    
    if(ci->bytesToRead == 0 && ci->tcpConnectionState == TCLOSED) { // no data to read, and connection closed? return E_EOF
        return E_EOF;
    }
    
    if(ci->tcpConnectionState == TLISTEN) {                         // if connection is still listening, return error E_LISTEN
        return E_LISTEN;
    }
    
    if(ci->bytesToRead == 0) {                                      // no data in host and in local buffer?
        return 0;
    }

    if(ci->bytesToRead > 0x7FFF) {                                  // if we can now receive more than 0x7FFF bytes, return just 0x7FFF, because otherwise it would look like negative error
        return 0x7FFF;
    } 
    
    // if have less than 0x7FFF, just return the value             
    return ci->bytesToRead;
}

//-------------------------------------
// data retrieval functions
int16 CNget_char(int16 handle)
{   
    if(!handle_valid(handle)) {                 // we don't have this handle? fail
        return E_BADHANDLE;
    }

    update_con_info(FALSE);                     // update connections info structs (max once per 100 ms)
    TConInfo *ci = &conInfo[handle];            // we're working with this connection
    
    if(ci->bytesToRead == 0) {                  // no data in host and in local buffer?
        return E_NODATA;
    }

    if(ci->tcpConnectionState == TCLOSED) {     // connection closed? return E_EOF
        return E_EOF;
    }

    // if no data buffered, read the data
    if(ci->charsGot == 0 || ci->charsUsed >= ci->charsGot) {
        commandLong[ 5] = NET_CMD_CNGET_CHAR;   // store function number 
        commandLong[ 6] = handle;				// store file handle 
        commandLong[10] = ci->charsUsed;        // how many chars were used

        // no more used chars and got chars
        ci->charsUsed   = 0;                    
        ci->charsGot    = 0;

        hdIf.cmd(ACSI_READ, commandLong, CMD_LENGTH_LONG, pDmaBuffer, 1);

        if(hdIf.success && hdIf.statusByte == E_NORMAL) {       // success?
            ci->charsGot = pDmaBuffer[0];                       // store the new count of chars we got
            memcpy(ci->chars, pDmaBuffer + 1, ci->charsGot);    // copy those bytes
        } else {                                                // fail?
            return E_NODATA;
        }
    }
    
    if(ci->charsUsed >= ci->charsGot) {         // still no data? fail
        return E_NODATA;
    }
    
    int value = ci->chars[ci->charsUsed];       // get the char
    ci->charsUsed++;                            // update used count
    return value;                               // return that char
}

NDB *CNget_NDB(int16 handle)
{
    if(!handle_valid(handle)) {             // we don't have this handle? fail
        return (NDB *) NULL;
    }

    update_con_info(FALSE);                 // update connections info structs (max once per 100 ms)
    TConInfo *ci = &conInfo[handle];        // we're working with this connection
    
    if(ci->tcpConnectionState == TCLOSED || ci->bytesToRead == 0) { // closed or nothing to read?
        return (NDB *) NULL;
    }

    //-----------------
    // first find out how big is the next NDB
  	commandLong[ 5] = NET_CMD_CNGET_NDB;    // store function number 
	commandLong[ 6] = handle;				// store file handle 
	commandLong[ 7] = 0;                    // get next NDB size
	commandLong[10] = ci->charsUsed;        // how many chars were used

    // no more used chars and got chars
    ci->charsUsed   = 0;                    
    ci->charsGot    = 0;

    hdIf.cmd(ACSI_READ, commandLong, CMD_LENGTH_LONG, pDmaBuffer, 1);

    if(!hdIf.success || hdIf.statusByte != E_NORMAL) {  // failed? quit
        return NULL;
    }
    
    // get size of NDB
    int  nexdNdbSizeBytes   = getDword(pDmaBuffer);     
    BYTE nextNdbSizeSectors = pDmaBuffer[4];
    
    //-----------------
    // then try to allocate the RAM for NDB
    DWORD readCount, freeSize;
    freeSize    = KRgetfree_internal(TRUE);                                     // get size of largest free block
    readCount   = (nexdNdbSizeBytes < freeSize) ? nexdNdbSizeBytes : freeSize;  // Do we have less data to read, then what we can KRmalloc()? If yes, read all, otherwise read just what we can KRmalloc()
    
    // allocate buffer for structure and the data
    BYTE *bfr = KRmalloc_internal(readCount);                               // try to malloc() RAM for data
    
    if(bfr == NULL) {                                                       // malloc() failed, return NULL
        return (NDB *) NULL;
    }
    
    NDB *pNdb = KRmalloc_internal(sizeof(NDB));                             // try to malloc() RAM for the containing NDB struct 
    
    if(pNdb == NULL) {                                                      // malloc() failed, return NULL
        KRfree_internal(bfr);                                               // free the data buffer
        return (NDB *) NULL;
    }

    //-----------------
    // setup the NDB structure - but using direct access, as calling code uses different packing than gcc
    storeDword(((BYTE *)pNdb) +  0, (DWORD) pNdb);          // pointer block start - for free()
    storeDword(((BYTE *)pNdb) +  4, (DWORD) bfr);           // pointer to data
    storeWord (((BYTE *)pNdb) +  8, (WORD)  readCount);     // length of data in buffer
    storeDword(((BYTE *)pNdb) + 10, (DWORD) 0);             // pointer to next NDB

    //-----------------
    // get the NDB data from host
   	commandLong[ 5] = NET_CMD_CNGET_NDB;    // store function number 
	commandLong[ 6] = handle;				// store file handle 
	commandLong[ 7] = 1;                    // get next NDB data
	commandLong[10] = 0;                    // how many chars were used

    hdIf.cmd(ACSI_READ, commandLong, CMD_LENGTH_LONG, bfr, nextNdbSizeSectors);

    if(!hdIf.success || hdIf.statusByte != E_NORMAL) {  // failed? quit
        KRfree_internal(bfr);
        KRfree_internal(pNdb);
        return (NDB *) NULL;
    }
    
    return pNdb;                                                            // return the pointer to NDB structure
}

int16 CNget_block(int16 handle, void *buffer, int16 length)
{
    if(!handle_valid(handle)) {             // we don't have this handle? fail
        return E_BADHANDLE;
    }

    update_con_info(FALSE);                 // update connections info structs (max once per 100 ms)
    TConInfo *ci = &conInfo[handle];        // we're working with this connection
    
    if(ci->tcpConnectionState == TCLOSED) { // no data to read, and connection closed? return E_EOF
        return E_EOF;
    }
    
    if(ci->bytesToRead < length) {          // not enough data to read the whole block? fail
        return E_NODATA;
    }
    
    if(length < 1) {                        // nothing to do? good
        return 0;
    }

    //----------------------------------------
    // no more used chars and got chars
    int charsUsed   = ci->charsUsed;
    ci->charsUsed   = 0;                    
    ci->charsGot    = 0;

    //----------------------------------------
    // how many sectors and bytes we need for long transfer
    int lenLongSectors  = length         >> 9;
    int lenLongBytes    = lenLongSectors << 9;
    
    // how many bytes we need for short transfer
    int lenShortBytes   = length      & 0x1ff;
    
    //----------------------------------------
    // first do the long transfer
    if(lenLongSectors > 0) {                    // if we should do the long transfer (something to transfer?)
        commandLong[ 5] = NET_CMD_CNGET_BLOCK;  // store function number 
        commandLong[ 6] = handle;				// store file handle 
        commandLong[ 7] = (BYTE) (lenLongBytes >> 8);
        commandLong[ 8] = (BYTE) (lenLongBytes     );
        commandLong[10] = charsUsed;            // how many chars were used

        hdIf.cmd(ACSI_READ, commandLong, CMD_LENGTH_LONG, buffer, lenLongSectors);

        if(!hdIf.success || hdIf.statusByte != E_NORMAL) {  // failed? quit
            return E_NODATA;
        }
    }
    //----------------------------------------
    // then do the short transfer
    if(lenShortBytes > 0) {                     // if we should do the short transfer (something to transfer?)
        commandLong[ 5] = NET_CMD_CNGET_BLOCK;  // store function number 
        commandLong[ 6] = handle;				// store file handle 
        commandLong[ 7] = (BYTE) (lenShortBytes >> 8);
        commandLong[ 8] = (BYTE) (lenShortBytes     );
        commandLong[10] = 0;                    // no chars were used

        hdIf.cmd(ACSI_READ, commandLong, CMD_LENGTH_LONG, pDmaBuffer, 1);

        if(!hdIf.success || hdIf.statusByte != E_NORMAL) {  // failed? quit
            return E_NODATA;
        }
        
        memcpy(buffer + lenLongBytes, pDmaBuffer, lenShortBytes);   // copy it from intermediate buffer to final buffer
    }
    //----------------------------------------
    
    return length;
}

int16 CNgets(int16 handle, char *buffer, int16 length, char delimiter)
{
    if(!handle_valid(handle)) {             // we don't have this handle? fail
        return E_BADHANDLE;
    }

    update_con_info(FALSE);                 // update connections info structs (max once per 100 ms)
    TConInfo *ci = &conInfo[handle];        // we're working with this connection
    
    if(ci->tcpConnectionState == TCLOSED) { // no data to read, and connection closed? return E_EOF
        return E_EOF;
    }
    
    if(ci->bytesToRead <= 2) {              // not enough data to read? fail
        return E_NODATA;
    }

    if(length < 1) {                        // nothing to do? good
        return E_BIGBUF;
    }    

    //-----------------------
    // limit the length
    length              = (length < DMA_BUFFER_SIZE) ? length : DMA_BUFFER_SIZE;
    int sectorLength    = length >> 9;
    //-----------------------
    // issue the command
  	commandLong[ 5] = NET_CMD_CNGETS;       // store function number 
	commandLong[ 6] = handle;				// store file handle 
	commandLong[ 7] = (BYTE) (length >> 8); // store max length
	commandLong[ 8] = (BYTE) (length     );
    commandLong[ 9] = delimiter;            // store delimiter
	commandLong[10] = ci->charsUsed;        // how many chars were used

    // no chars used and got no chars
    ci->charsUsed   = 0;                    
    ci->charsGot    = 0;
    
    hdIf.cmd(ACSI_READ, commandLong, CMD_LENGTH_LONG, pDmaBuffer, sectorLength);

    if(!hdIf.success) {                     // communication failed? quit
        return E_NODATA;
    }
    
    if(hdIf.statusByte == E_NORMAL) {       // got the string? good
        memcpy(buffer, pDmaBuffer, length); // copy it to buffer
        return E_NORMAL;
    }

    // in other cases - just return the status byte
    return hdIf.statusByte;
}

//--------------------------------------
// helper functions

int handle_valid(int16 h)
{
    if(h < 0 && h >= MAX_HANDLE) {                                          // handle out of range?
        return FALSE;
    }
    
    WORD proto = getCIBitem((BYTE *) &conInfo[h].cib, CIB_PROTO);
    
    if(proto == 0) {                // if connection not used
        return FALSE;
    }
    
    return TRUE;
}

void update_con_info(BYTE forceUpdate)
{
	static DWORD lastUpdate = 0;
	DWORD now = getTicks();

    if(!forceUpdate) {                                                      // if shouldn't force update, check last update time, and possibly ignore update
        if((now - lastUpdate) < 20) {								        // if the last update was less than 100 ms ago, don't update
            return;
        }
    }
	
	lastUpdate = now;											            // mark that we've just updated the ceDrives 
	
	// now do the real update 
	commandShort[4] = NET_CMD_CN_UPDATE_INFO;								// store function number 
	commandShort[5] = 0;										
	
	hdIf.cmd(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);   // send command to host over ACSI 
	
    if(!hdIf.success || hdIf.statusByte != E_NORMAL) {							                                // error? 
		return;														
	}
    
    // now update our data from received data    
    int i;
    DWORD   *pBytesToRead       = (DWORD *)  pDmaBuffer;                        // offset   0: 32 * 4 bytes - bytes to read for each connection
    BYTE    *pConnStatus        = (BYTE  *) (pDmaBuffer + 128);                 // offset 128: 32 * 1 bytes - connection status
    WORD    *pLPort             = (WORD  *) (pDmaBuffer + 160);                 // offset 160: 32 * 2 bytes - local port
    DWORD   *pRHost             = (DWORD *) (pDmaBuffer + 224);                 // offset 224: 32 * 4 bytes - remote host
    WORD    *pRPort             = (WORD  *) (pDmaBuffer + 352);                 // offset 352: 32 * 2 bytes - remote port
    DWORD   *pBytesToReadIcmp   = (DWORD *) (pDmaBuffer + 416);                 // offset 416:  1 * 1 DWORD - bytes that can be read from ICMP socket(s)
    
    for(i=0; i<MAX_HANDLE; i++) {                                               // retrieve all the data and fill the variables
        // retrieve and update internal vars
        TConInfo *ci = &conInfo[i];
        
        ci->bytesToRead         = (DWORD)   pBytesToRead[i];
        ci->tcpConnectionState  = (BYTE)    pConnStatus[i];
        
        WORD  lPort = (WORD )   pLPort[i];
        DWORD rHost = (DWORD)   pRHost[i];
        WORD  rPort = (WORD)    pRPort[i];
        
        // CIB update through helper functions because of Pure C vs gcc packing of structs
        // Update      : local port, remote port, remote host, status
        // Don't update: protocol, local host
        setCIB((BYTE *) &ci->cib, NO_CHANGE_W, lPort, rPort, rHost, NO_CHANGE_DW, /* pConnStatus[i] */ 0);
    }
    
    DWORD bytesToReadIcmp = (DWORD) *pBytesToReadIcmp;                          // get how many bytes we can read from ICMP socket(s)
    
    if(bytesToReadIcmp > 0) {                                                   // if we have something for ICMP to process?
        icmp_processData(bytesToReadIcmp);
    }
}

//-------------------------------------------------------------------------------

int16 connection_open(int tcpNotUdp, uint32 rem_host, uint16 rem_port, uint16 tos, uint16 buff_size)
{
    // first store command code
    if(tcpNotUdp) {                         // open TCP
        commandShort[4] = NET_CMD_TCP_OPEN;
    } else {                                // open UDP
        commandShort[4] = NET_CMD_UDP_OPEN;
    }
    
    commandShort[5] = 0;

    WORD  lPort = 0;
    DWORD lHost = 0;
    
    //--------------------------
    // find out if it's active or passive connection
    BYTE activeNotPassive = TRUE;       // active by default
    
    if(tcpNotUdp) {                     // for TCP
        if(rem_host == 0 || rem_port == TCP_PASSIVE) {      // if no remote host specified, or specified TCP_PASSIVE, it's passive connection
            activeNotPassive = FALSE;
        }
    }
    //--------------------------
    
    if(rem_port == TCP_ACTIVE || rem_port == TCP_PASSIVE) {     // if the remote port is special flag (passive or active), then remote host contains pointer to CAB structure
        if(rem_host != 0) {                                     // it's not a NULL pointer
            BYTE *pCAB = (BYTE *) rem_host;                     // convert int to pointer and retrieve struct values
            
            lPort       = getWord (pCAB + 0);
            rem_port    = getWord (pCAB + 2);
            rem_host    = getDword(pCAB + 4);
            lHost       = getDword(pCAB + 8);
        }
    }
    
    // then store the params in buffer
    BYTE *pBfr = pDmaBuffer;
    pBfr = storeDword   (pBfr, rem_host);       //  0 ..  3
    pBfr = storeWord    (pBfr, rem_port);       //  4 ..  5
    pBfr = storeWord    (pBfr, tos);            //  6 ..  7
    pBfr = storeWord    (pBfr, buff_size);      //  8 ..  9
    pBfr = storeDword   (pBfr, lHost);          // 10 .. 13
    pBfr = storeWord    (pBfr, lPort);          // 14 .. 15

    // send it to host
    hdIf.cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);

    if(handleIsFromCE(hdIf.statusByte)) {                   // if it's CE handle
        int stHandle = handleCEtoAtari(hdIf.statusByte);    // convert it to ST handle
        TConInfo *ci = &conInfo[stHandle];
        
        int proto, status;
        
        // store info to CIB and CAB structures
        if(tcpNotUdp) {
            proto = TCP;
        } else {
            proto = UDP;
        }
        
        DWORD local_host;
        if(activeNotPassive) {      // active connection - trying to connect now, local host is local IP
            status      = TSYN_SENT;
            local_host  = localIP;
        } else {                    // passive connection - listening, local host is 0
            status      = TLISTEN;
            local_host  = 0;
        }

        // local port to 0 - we currently don't know that (yet)
        setCIB((BYTE *) &ci->cib, proto, lPort, rem_port, rem_host, local_host, status);
        update_con_info(TRUE);                      // let's FORCE update connection info - CIB should be updated with real local port after this (e.g. aFTP relies on the info when using active ftp connection)
        
        ci->buff_size           = buff_size;        // store the maximu buffer size fot TCP
        ci->activeNotPassive    = activeNotPassive; // store active/passive flag
        
        return stHandle;                            // return the new handle
    } 

    // it's not a CE handle
    return extendByteToWord(hdIf.statusByte);       // extend the BYTE error code to WORD
}

//-------------------------------------------------------------------------------

int16 connection_close(int tcpNotUdp, int16 handle, int16 timeout)
{
    if(!handle_valid(handle)) {                     // we don't have this handle? fail
        return E_BADHANDLE;
    }
    
    // first store command code
    if(tcpNotUdp) {                         // close TCP
        commandShort[4] = NET_CMD_TCP_CLOSE;
    } else {                                // close UDP
        commandShort[4] = NET_CMD_UDP_CLOSE;
    }
    
    commandShort[5] = 0;

    // then store the params in buffer
    BYTE *pBfr = pDmaBuffer;
    pBfr = storeWord    (pBfr, handle);
    pBfr = storeWord    (pBfr, timeout);

    // send it to host
    hdIf.cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);

    // don't handle failures, just pretend it's always closed just fine

    setCIB((BYTE *) &conInfo[handle].cib, 0, 0, 0, 0, 0, 0);     // clear the CIB structure
    return E_NORMAL;
}

//-------------------------------------------------------------------------------

int16 connection_send(int tcpNotUdp, int16 handle, void *buffer, int16 length)
{
    if(!handle_valid(handle)) {                     // we don't have this handle? fail
        return E_BADHANDLE;
    }
    
    if(tcpNotUdp) {                                 // if it's TCP connection...
        if(length > conInfo[handle].buff_size) {    // ...and trying to send more than specified buffer size in TCP_open(), fail
            return E_OBUFFULL;
        }
    }
    
    // first store command code
    BYTE netCmd;
    if(tcpNotUdp) {                         // for TCP
        netCmd = NET_CMD_TCP_SEND;
    } else {                                // for UDP
        netCmd = NET_CMD_UDP_SEND;
    }
    
    // transfer the remaining data in a loop 
    BYTE  toFastRam = (((int)buffer) >= 0x1000000) ? TRUE : FALSE;          // flag: are we reading to FAST RAM?
    DWORD blockSize = toFastRam ? FASTRAM_BUFFER_SIZE : (MAXSECTORS * 512); // size of block, which we will read
    
    while(length > 0) {										    // while there's something to send 
        // calculate how much data we should transfer in this loop - with respect to MAX SECTORS we can transfer at once 
        DWORD thisWriteSizeBytes = (length < blockSize) ? length : blockSize; // will the needed write size within the blockSize, or not?
    
        if(toFastRam) {     // if writing from FAST RAM, first cop to fastRamBuffer, and then write using DMA
            memcpy(FastRAMBuffer, buffer, thisWriteSizeBytes);
            connection_send_block(netCmd, handle, thisWriteSizeBytes, FastRAMBuffer);
        } else {            // if writing from ST RAM, just writing directly there
            connection_send_block(netCmd, handle, thisWriteSizeBytes, buffer);
        }

        buffer			+= thisWriteSizeBytes;					// and move pointer in buffer further 
        length		    -= thisWriteSizeBytes;					// decrease the count that is remaining 

        if(!hdIf.success || hdIf.statusByte != E_NORMAL) {      // if failed, return FALSE 
            return E_LOSTCARRIER;
        }
    }		
    
    return extendByteToWord(hdIf.statusByte);           // return the status, possibly extended to int16
}

void connection_send_block(BYTE netCmd, BYTE handle, WORD length, BYTE *buffer)
{
    commandLong[5] = netCmd;                        // store command code
    commandLong[6] = handle;                        // store handle

    commandLong[7] = (BYTE) (length >> 8);          // cmd[7 .. 8]  = length
    commandLong[8] = (BYTE) (length     );

    // prepare the command for buffer sending - add flag 'buffer address is odd' and possible byte #0, in case the buffer address was odd
    BYTE *pBfr  = (BYTE *) buffer;
    DWORD dwBfr = (DWORD)  buffer;

    if(dwBfr & 1) {                                 // buffer pointer is ODD
        commandLong[9]  = TRUE;                     // buffer is odd
        commandLong[10] = pBfr[0];                  // byte #0 is cmd[10]
        pBfr++;                                     // and pBfr is now EVEN
    } else {                                        // buffer pointer is EVEN
        commandLong[9]  = FALSE;                    // buffer is even
        commandLong[10] = 0;                        // this is zero, not byte #0
    }    
    
    // calculate sector count
    WORD sectorCount = length / 512;                // get number of sectors we need to send
    
    if((length % 512) != 0) {                       // if the number of bytes is not multiple of 512, then we need to send one sector more
        sectorCount++;
    }
    
    // send it to host
    hdIf.cmd(ACSI_WRITE, commandLong, CMD_LENGTH_LONG, pBfr, sectorCount);
}

//-------------------------------------------------------------------------------

int16 resolve (char *domain, char **real_domain, uint32 *ip_list, int16 ip_num)
{
    commandShort[4] = NET_CMD_RESOLVE;
    commandShort[5] = 0;
    
    if(domain == NULL) {                                                // if nothing to resolve, quit
        return E_CANTRESOLVE;
    }
    
    strcpy((char *) pDmaBuffer, domain);                                // copy in the domain or dotted quad IP address

    // send it to host
    hdIf.cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);

	if(!hdIf.success) {
		return E_CANTRESOLVE;
	}
    
    if(hdIf.statusByte >= 10) {                                          // 0 .. 9 is handle, 10 and more means error
        return E_CANTRESOLVE;
    }
    
    BYTE resolverHandle = hdIf.statusByte;                              // this is the new handle for resolver

    // now receive the response
    memset(pDmaBuffer, 0, 512);
    commandShort[4] = NET_CMD_RESOLVE_GET_RESPONSE;
    commandShort[5] = resolverHandle;
    
    DWORD end = getTicks() + 10 * 200;                                  // 10 second timeout
    
    while(1) {                                                          // repeat this command few times, as it might reply with 'I didn't finish yet'
        sleepMs(250);                                               // wait 250 ms before trying again
        if(getTicks() >= end) {                                         // if timeout
            return E_CANTRESOLVE;
        }
    
        hdIf.cmd(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);

        if(hdIf.statusByte == E_NORMAL) {                               // if finished, continue after loop
            break;
        }

        if(hdIf.statusByte == RES_DIDNT_FINISH_YET) {                   // if not finished, try again
            continue;
        }
    }
    
    // now copy the list of IPs to ip_list
    int ipListCount = (int) pDmaBuffer[256];                            // get how many IPs we got from host
    int ipCount     = (ip_num < ipListCount) ? ip_num : ipListCount;    // get the lower count - either what we may store (ip_num) or what we found (ipListCount)
    
    int i;
    uint32 *pIPs = (uint32 *) &pDmaBuffer[258];
    for(i=0; i<ipCount; i++) {                                          // copy all the IPs
        ip_list[i] = pIPs[i];
    }
    
    if(ipCount == 0) {                                                  // no IPs? can't resolve
        return E_CANTRESOLVE;
    }

    // possibly copy the real domain name
    if(real_domain != NULL) {                                           // if got pointer to real domain
        char *realName = KRmalloc_internal(512);
        if(!realName) {                                                 // failed to allocate RAM?
            return E_NOMEM;
        }
    
        strcpy(realName, (char *) pDmaBuffer);                          // copy in the real name
        *real_domain = realName;                                        // store the pointer to where the real domain should be
    }
    
    return ipCount;                                                     // Returns the number of dotted quad IP addresses filled in, or an error.
}

//-------------------------------------------------------------------------------
static void initConInfoStruct(int i)
{
    if(i >= MAX_HANDLE) {
        return;
    }

    TConInfo *ci = &conInfo[i];
    
    ci->bytesToRead          = 0;
    ci->tcpConnectionState   = TCLOSED;
    ci->charsUsed            = 0;
    ci->charsGot             = 0;
    setCIB((BYTE *) &ci->cib, 0, 0, 0, 0, 0, 0);    // clear the CIB structure
    memset(ci->chars, 0, READ_BUFFER_SIZE);
}
//-------------------------------------------------------------------------------
void init_con_info(void)
{
    int i;
    
    for(i=0; i<MAX_HANDLE; i++) {
        initConInfoStruct(i);
    }
}
//-------------------------------------------------------------------------------
void setCIB(BYTE *cib, WORD protocol, WORD lPort, WORD rPort, DWORD rHost, DWORD lHost, WORD status)
{
    // first create pointers to the right addresses
    WORD  *pProto   = (WORD  *) (cib +  0);
    WORD  *plPort   = (WORD  *) (cib +  2);
    WORD  *prPort   = (WORD  *) (cib +  4);
    DWORD *prHost   = (DWORD *) (cib +  6);
    DWORD *plHost   = (DWORD *) (cib + 10);
    WORD  *pStatus  = (WORD  *) (cib + 14);
    
    // now if we should set the new value, set it
    if(protocol != NO_CHANGE_W)     *pProto     = protocol;
    if(lPort    != NO_CHANGE_W)     *plPort     = lPort;
    if(rPort    != NO_CHANGE_W)     *prPort     = rPort;
    if(rHost    != NO_CHANGE_DW)    *prHost     = rHost;
    if(lHost    != NO_CHANGE_DW)    *plHost     = lHost;
    if(status   != NO_CHANGE_W)     *pStatus    = status;
}
//-------------------------------------------------------------------------------
DWORD getCIBitem(BYTE *cib, BYTE item)
{
    // first create pointers to the right addresses
    WORD  *pProto   = (WORD  *) (cib +  0);
    WORD  *plPort   = (WORD  *) (cib +  2);
    WORD  *prPort   = (WORD  *) (cib +  4);
    DWORD *prHost   = (DWORD *) (cib +  6);
    DWORD *plHost   = (DWORD *) (cib + 10);
    WORD  *pStatus  = (WORD  *) (cib + 14);
    
    switch(item) {
        case CIB_PROTO:     return *pProto; 
        case CIB_LPORT:     return *plPort;
        case CIB_RPORT:     return *prPort;
        case CIB_RHOST:     return *prHost;
        case CIB_LHOST:     return *plHost;
        case CIB_STATUS:    return *pStatus;
    }
    
    return 0;
}
//-------------------------------------------------------------------------------

