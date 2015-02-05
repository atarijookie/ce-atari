#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <support.h>
#include <stdint.h>
#include <stdio.h>

#include "globdefs.h"
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
    update_con_info();                  // update connections info structs (max once per 100 ms)

    if(!handle_valid(handle)) {         // we don't have this handle? fail
        return E_BADHANDLE;
    }

	return E_NORMAL;
}

CIB *CNgetinfo(int16 handle)
{
    // Note: do return valid CIB* even for closed handles, the client app might expect this to be not NULL even if the connection is opening / closing / whatever

    if(handle < 0 || handle >= MAX_HANDLE) {    // handle out of range? fail
        return (CIB *) NULL;
    }

    update_con_info();                  // update connections info structs (max once per 100 ms)

	return &conInfo[handle].cib;        // return pointer to correct CIB
}

int16 CNbyte_count (int16 handle)
{
    if(!handle_valid(handle)) {                 // we don't have this handle? fail
        return E_BADHANDLE;
    }

    update_con_info();                                              // update connections info structs (max once per 100 ms)
    TConInfo    *ci             = &conInfo[handle];                 // we're working with this connection
    DWORD       dataLeftLocal   = ci->rCount - ci->rStart;          // calculate how much data we have in local buffer
    DWORD       dataLeftTotal   = dataLeftLocal + ci->bytesToRead;  // total data left = local data count + host data count
    
    if(dataLeftTotal == 0 && ci->tcpConnectionState == TCLOSED) {   // no data to read, and connection closed? return E_EOF
        return E_EOF;
    }
    
    if(ci->tcpConnectionState == TLISTEN) {                         // if connection is still listening, return error E_LISTEN
        return E_LISTEN;
    }
    
    if(dataLeftTotal == 0) {                                        // no data in host and in local buffer?
        return 0;
    }

    if(dataLeftTotal > 0x7FFF) {                                    // if we can now receive more than 0x7FFF bytes, return just 0x7FFF, because otherwise it would look like negative error
        return 0x7FFF;
    } 
    
    // if have less than 0x7FFF, just return the value             
    return dataLeftTotal;
}

//-------------------------------------
// data retrieval functions
int16 CNget_char(int16 handle)
{   
    if(!handle_valid(handle)) {                 // we don't have this handle? fail
        return E_BADHANDLE;
    }

    update_con_info();                                              // update connections info structs (max once per 100 ms)
    TConInfo    *ci             = &conInfo[handle];                 // we're working with this connection
    DWORD       dataLeftLocal   = ci->rCount - ci->rStart;          // calculate how much data we have in local buffer
    DWORD       dataLeftTotal   = dataLeftLocal + ci->bytesToRead;  // total data left = local data count + host data count
    
    if(dataLeftTotal == 0 && ci->tcpConnectionState == TCLOSED) {   // no data to read, and connection closed? return E_EOF
        return E_EOF;
    }
    
    if(dataLeftTotal == 0) {                                        // no data in host and in local buffer?
        return E_NODATA;
    }
    
    // if there is data...    
    if(dataLeftLocal == 0) {                                        // ...but we don't have data in local read buffer? read it
        fillReadBuffer(handle);

        dataLeftLocal = ci->rCount - ci->rStart;
        if(dataLeftLocal == 0) {                                    // this shouldn't happen - no data after read
            return E_NODATA;
        }
    }
    
    // get the byte and return it
    int16 value = ci->rBuf[ci->rStart];         
    ci->rStart++;
    
    return value;
}

NDB *CNget_NDB (int16 handle)
{
    if(!handle_valid(handle)) {                                             // we don't have this handle? fail
        return (NDB *) NULL;
    }

    update_con_info();                                                      // update connections info structs (max once per 100 ms)
    TConInfo    *ci             = &conInfo[handle];                         // we're working with this connection
    DWORD       dataLeftLocal   = ci->rCount - ci->rStart;                  // calculate how much data we have in local buffer
    DWORD       dataLeftTotal   = dataLeftLocal + ci->bytesToRead;          // total data left = local data count + host data count
    
    if(dataLeftTotal == 0 && ci->tcpConnectionState == TCLOSED) {   // no data to read, and connection closed? return E_EOF
        return (NDB *) E_EOF;
    }

    if(dataLeftTotal == 0) {                                                // no data in host and in local buffer?
        return (NDB *) E_NODATA;
    }

    // if we got here, then there's some data to retrieve...
    DWORD readCount, freeSize;
    freeSize    = KRgetfree_internal(TRUE);                                 // get size of largest free block
    readCount   = (dataLeftTotal    <= 0xffff)   ? dataLeftTotal : 0xffff;  // Do we have less than 64kB of data? Retrieve all, otherwise retrieve just 64kB.
    readCount   = (readCount        <= freeSize) ? readCount : freeSize;    // Do we have less data to read, then what we can KRmalloc()? If yes, read all, otherwise read just what we can KRmalloc()
    
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

    // setup the NDB structure - but using direct access, as calling code uses different packing than gcc
    storeDword(((BYTE *)pNdb) +  0, (DWORD) pNdb);          // pointer block start - for free()
    storeDword(((BYTE *)pNdb) +  4, (DWORD) bfr);           // pointer to data
    storeWord (((BYTE *)pNdb) +  8, (WORD)  readCount);     // length of data in buffer
    storeDword(((BYTE *)pNdb) + 10, (DWORD) 0);             // pointer to next NDB
    
    // now do the actual transfer
    DWORD res = 0;
    if(readCount <= (2*READ_BUFFER_SIZE)) {                                 // less than 2 local buffers? 
        res = read_small(handle, readCount, (BYTE *) bfr);                  // do small read
    } else {
        res = read_big(handle, readCount, (BYTE *) bfr);                    // do big read
    }

    if(res == 0) {                                                          // if failed, fail
        KRfree_internal(bfr);
        KRfree_internal(pNdb);
        return (NDB *) NULL;
    }
    
    storeWord (((BYTE *)pNdb) +  8, (WORD) res);            // store how many bytes we actually did read
    return pNdb;                                                            // return the pointer to NDB structure
}

int16 CNget_block(int16 handle, void *buffer, int16 length)
{
    if(!handle_valid(handle)) {                 // we don't have this handle? fail
        return E_BADHANDLE;
    }

    update_con_info();                                              // update connections info structs (max once per 100 ms)
    TConInfo    *ci             = &conInfo[handle];                 // we're working with this connection
    DWORD       dataLeftLocal   = ci->rCount - ci->rStart;          // calculate how much data we have in local buffer
    DWORD       dataLeftTotal   = dataLeftLocal + ci->bytesToRead;  // total data left = local data count + host data count
    
    if(dataLeftTotal == 0 && ci->tcpConnectionState == TCLOSED) {   // no data to read, and connection closed? return E_EOF
        return E_EOF;
    }
    
    if(dataLeftTotal == 0) {                                        // no data in host and in local buffer?
        return E_NODATA;
    }

    if(dataLeftTotal < length) {                                    // not enough data to read the whole block? fail
        return E_NODATA;
    }
    
    int16 res = 0;
    if(length <= (2*READ_BUFFER_SIZE)) {                            // less than 2 local buffers? 
        res = read_small(handle, length, (BYTE *) buffer);          // do small read
    } else {
        res = read_big(handle, length, (BYTE *) buffer);            // do big read
    }
    
    return res;
}

int16 CNgets(int16 handle, char *buffer, int16 length, char delimiter)
{
    if(!handle_valid(handle)) {                                     // we don't have this handle? fail
        return E_BADHANDLE;
    }

    update_con_info();                                              // update connections info structs (max once per 100 ms)
    TConInfo    *ci             = &conInfo[handle];                 // we're working with this connection
    DWORD       dataLeftLocal   = ci->rCount - ci->rStart;          // calculate how much data we have in local buffer
    DWORD       dataLeftTotal   = dataLeftLocal + ci->bytesToRead;  // total data left = local data count + host data count
    
    if(dataLeftTotal == 0 && ci->tcpConnectionState == TCLOSED) {   // no data to read, and connection closed? return E_EOF
        return E_EOF;
    }
    
    if(dataLeftTotal == 0) {                                        // no data in host and in local buffer?
        return E_NODATA;
    }

    //-----------------------
    // before we do anything, we should check if the string could fit in the provided buffer
    int32 i, realLen = -1;
    
    for(i=ci->rStart; i<ci->rCount; i++) {                          // check if the local buffer has this delimiter
        if(ci->rBuf[i] == delimiter) {                              // delimiter found, quit the search
            realLen = i - ci->rStart;
            break;
        }
    }

    if(realLen == -1) {                                                                     // we didn't find the delimiter, try to search for it in host
    	commandLong[5] = NET_CMD_CN_LOCATE_DELIMITER;
		commandLong[6] = handle;
        commandLong[7] = delimiter;
    
		BYTE res = acsi_cmd(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);      // send command to host over ACSI

		if(res != E_NORMAL) {                                                               // failed? say that no data was transfered
			return E_NODATA;
		}
        
        DWORD lenOnHost = getDword(pDmaBuffer);                                             // read the position on host
        
        if(lenOnHost == DELIMITER_NOT_FOUND) {                                              // not found? fail
            return E_NODATA;                                                                // not enough data in the input buffer to find the delimiter
        }
        
        realLen = dataLeftLocal + lenOnHost;                                                // found on host, so real length is local length + length on host
    }
    
    // now verify if the provided buffer is long enough
    if(length < realLen) {
        return E_BIGBUF;                                                                    // return: the buffer is not large enough to hold the whole block of data
    }
    
    //-----------------------
    // if we got here, we know that we do have that delimiter and the buffer is large enough to get it, 
    // so get it using small buffers
    DWORD cnt = read_small(handle, realLen, (BYTE *) buffer);
    
    if(cnt != realLen) {                // if failed to read all required data (shouldn't happen), fail
        return E_NODATA;
    }
    
    buffer[realLen - 1] = 0;            // terminate the string by removing the delimiter
	return (realLen - 1);               // returns the number of bytes read until the delimiter was found, i.e. the length of the buffer contents without the final '\0' byte
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

void update_con_info(void)
{
	DWORD res;
	static DWORD lastUpdate = 0;
	DWORD now = getTicks();

	if((now - lastUpdate) < 20) {								            // if the last update was less than 100 ms ago, don't update
		return;
	}
	
	lastUpdate = now;											            // mark that we've just updated the ceDrives 
	
	// now do the real update 
	commandShort[4] = NET_CMD_CN_UPDATE_INFO;								// store function number 
	commandShort[5] = 0;										
	
	res = acsi_cmd(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);   // send command to host over ACSI 
	
    if(res != OK) {							                                // error? 
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
        conInfo[i].bytesToRead          = (DWORD)   pBytesToRead[i];
        conInfo[i].tcpConnectionState   = (BYTE)    pConnStatus[i];
        
        WORD  lPort = (WORD )   pLPort[i];
        DWORD rHost = (DWORD)   pRHost[i];
        WORD  rPort = (WORD)    pRPort[i];
        
        // CIB update through helper functions because of Pure C vs gcc packing of structs
        // Update      : local port, remote port, remote host, status
        // Don't update: protocol, local host
        setCIB((BYTE *) &conInfo[i].cib, NO_CHANGE_W, lPort, rPort, rHost, NO_CHANGE_DW, pConnStatus[i]);
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
    
    // then store the params in buffer
    BYTE *pBfr = pDmaBuffer;
    pBfr = storeDword   (pBfr, rem_host);
    pBfr = storeWord    (pBfr, rem_port);
    pBfr = storeWord    (pBfr, tos);
    pBfr = storeWord    (pBfr, buff_size);

    // send it to host
    BYTE res = acsi_cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);

    if(handleIsFromCE(res)) {                       // if it's CE handle
        int stHandle = handleCEtoAtari(res);        // convert it to ST handle
        int proto, status;
        
        // store info to CIB and CAB structures
        if(tcpNotUdp) {
            proto = TCP;
        } else {
            proto = UDP;
        }
        
        if(rem_host == 0) {                         // if no remote host specified, it's a passive (listening) socket - listening for connection
            status = TLISTEN;
        } else {                                    // if remote host was specified, it's an active (outgoing) socket - trying to connect
            status = TSYN_SENT;
        }

        // local port to 0 - we currently don't know that (yet)
        setCIB((BYTE *) &conInfo[stHandle].cib, proto, 0, rem_port, rem_host, localIP, status);
        
        return stHandle;                            // return the new handle
    } 

    // it's not a CE handle
    return extendByteToWord(res);                   // extend the BYTE error code to WORD
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
    acsi_cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);

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
    
    // first store command code
    if(tcpNotUdp) {                         // for TCP
        commandLong[5] = NET_CMD_TCP_SEND;
    } else {                                // for UDP
        commandLong[5] = NET_CMD_UDP_SEND;
    }

    // then store the params in command part
    commandLong[6] = (BYTE) handle;                 // cmd[6]       = handle

    commandLong[7] = (BYTE) (length >> 8);          // cmd[7 .. 8]  = length
    commandLong[8] = (BYTE) (length     );

    // prepare the command for buffer sending - add flag 'buffer address is odd' and possible byte #0, in case the buffer address was odd
    BYTE *pBfr  = (BYTE *) buffer;
    DWORD dwBfr = (DWORD) buffer;

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
    BYTE res = acsi_cmd(ACSI_WRITE, commandLong, CMD_LENGTH_LONG, pBfr, sectorCount);


	if(res != OK) {                                 // if failed, return FALSE 
		return E_LOSTCARRIER;
	}

    return extendByteToWord(res);                   // return the status, possibly extended to int16
}

//-------------------------------------------------------------------------------

int16 resolve (char *domain, char **real_domain, uint32 *ip_list, int16 ip_num)
{
    BYTE res;
    
    commandShort[4] = NET_CMD_RESOLVE;
    commandShort[5] = 0;
    
    strcpy((char *) pDmaBuffer, domain);                                // copy in the domain or dotted quad IP address

    // send it to host
    res = acsi_cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);

	if(res != OK) {                                                     // if failed, return FALSE 
		return E_CANTRESOLVE;
	}

    // now receive the response
    memset(pDmaBuffer, 0, 512);
    commandShort[4] = NET_CMD_RESOLVE_GET_RESPONSE;
    
    while(1) {                                                          // repeat this command few times, as it might reply with 'I didn't finish yet'
        res = acsi_cmd(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);

        if(res == RES_DIDNT_FINISH_YET) {                               // if not finished, try again
            sleepMs(250);                                               // wait 250 ms before trying again
            continue;
        }
    
        if(res != OK) {                                                 // if failed, return FALSE 
            return E_CANTRESOLVE;
        }
        
        break;                                                          // if came here, success and finished, quit this loop
    }

    // possibly copy the real domain name
    if(real_domain != NULL) {                                           // if got pointer to real domain
        *real_domain = (char *) pDmaBuffer;                             // store the pointer to where the real domain should be
    }
    
    // now copy the list of IPs to ip_list
    int ipListCount = (int) pDmaBuffer[256];                            // get how many IPs we got from host
    int ipCount     = (ip_num < ipListCount) ? ip_num : ipListCount;    // get the lower count - either what we may store (ip_num) or what we found (ipListCount)
    
    int i;
    uint32 *pIPs = (uint32 *) &pDmaBuffer[258];
    for(i=0; i<ipCount; i++) {                                          // copy all the IPs
        ip_list[i] = pIPs[i];
    }
    
    return ipCount;                                                     // Returns the number of dotted quad IP addresses filled in, or an error.
}

//-------------------------------------------------------------------------------
static void initConInfoStruct(int i)
{
    if(i >= MAX_HANDLE) {
        return;
    }

    conInfo[i].bytesToRead          = 0;
    conInfo[i].tcpConnectionState   = TCLOSED;
    conInfo[i].rCount               = 0;
    conInfo[i].rStart               = 0;
    setCIB((BYTE *) &conInfo[i].cib, 0, 0, 0, 0, 0, 0);     // clear the CIB structure
    memset(conInfo[i].rBuf, 0, READ_BUFFER_SIZE);
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
BYTE fillReadBuffer(int16 handle)
{
	DWORD res;

	if(handle >= MAX_HANDLE) {                                        // would be out of index? quit - with error 
		return FALSE;
	}

    TConInfo *ci = &conInfo[handle];
	ci->rCount = 0;
	ci->rStart = 0;
	
	res = readData(handle, ci->rBuf, 512, 0);                       // try to read 512 bytes

	ci->rCount = res;                                               // store how much data we've read
    
    if(res <= ci->bytesToRead) {                                    // update how many bytes there are on host to read, if the result is >= 0
        ci->bytesToRead -= res;
    } else {                                                        // update how many bytes there are on host - the result is 0
        ci->bytesToRead = 0;
    }
    
	return TRUE;
}
//-------------------------------------------------------------------------------
DWORD readData(int16 handle, BYTE *bfr, DWORD cnt, BYTE seekOffset)
{
	commandLong[5] = NET_CMD_CN_READ_DATA;      // store function number 
	commandLong[6] = handle;					// store file handle 
	
	commandLong[10] = seekOffset;				// seek offset before read
	
	WORD sectorCount = cnt / 512;				// calculate how many sectors should we transfer 
	DWORD count=0;

	if((cnt % 512) != 0) {                      // and if we have more than full sector(s) in buffer, send one more! 
		sectorCount++;
	}

	if ((int)bfr>=0x1000000)                    // Oh dear, are we out of ST RAM boundaries? The ACSI DMA won't read past 0xffffff
	{
		DWORD cnt_remain=cnt;												
		DWORD actual_bytes_read=0;
		DWORD bytes_to_read;

		// In the case of reading outside ST RAM, we need to read the 
		// data in an inbetween buffer and the copy it over.
		// But we can't afford to read all data in one go because we
		// would need an equal sized buffer. Instead, read the data in
		// chunks and copy it over.
		
		while (cnt_remain>0)
		{
			if (cnt_remain>FASTRAM_BUFFER_SIZE)
				bytes_to_read=FASTRAM_BUFFER_SIZE;
			else
				bytes_to_read=cnt_remain;

			commandLong[7] = bytes_to_read >> 16;											                // store byte count 
			commandLong[8] = bytes_to_read >>  8;
			commandLong[9] = bytes_to_read  & 0xff;

            sectorCount = bytes_to_read / 512;                                                              // calculate how many sectors should we transfer 
            if((bytes_to_read % 512) != 0) {                                                                // and if we have more than full sector(s) in buffer, send one more! 
                sectorCount++;
            }
            
			BYTE res = acsi_cmd(ACSI_READ, commandLong, CMD_LENGTH_LONG, FastRAMBuffer, sectorCount);	    // Read as much as we can to ST RAM first - send command to host over ACSI 

			if(res == RW_ALL_TRANSFERED) {
				memcpy(bfr, FastRAMBuffer, bytes_to_read);                                                  // Yup, so copy data to its rightful place
				bfr=bfr+FASTRAM_BUFFER_SIZE;
				actual_bytes_read = actual_bytes_read + bytes_to_read;
				cnt_remain=cnt_remain-bytes_to_read;
			}
			else
			{
				// if the result is also not partial transfer, then some other error happened, return that no data was transfered
				if(res != RW_PARTIAL_TRANSFER ) {
					return 0;	
				}

				// if we got here, then partial transfer happened, see how much data we got
				commandShort[4] = NET_CMD_CN_GET_DATA_COUNT;
				commandShort[5] = handle;										
				res = acsi_cmd(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);   // send command to host over ACSI
		
				if(res != E_NORMAL) {                                                       // failed? say that no data was transfered
					return 0;
				}
		
		    	count = getDword(pDmaBuffer);                                               // read how much data was read
				memcpy(bfr, FastRAMBuffer, count);                                          // Yup, so copy data to its rightful place
				return actual_bytes_read+count;
			}
		}
		count= actual_bytes_read;
	}
	else
	{
		// We're inside ST RAM, so proceed normally (i.e. get the ASCI to DMA the data from the disk to RAM)
	
		commandLong[7] = cnt >> 16;                                                         // store byte count 
		commandLong[8] = cnt >>  8;
		commandLong[9] = cnt  & 0xff;
	
		BYTE res = acsi_cmd(ACSI_READ, commandLong, CMD_LENGTH_LONG, bfr, sectorCount);     // Normal read to ST RAM - send command to host over ACSI 
	
		// if all data transfered, return count of all data
		if(res == RW_ALL_TRANSFERED) {
			return cnt;
		}

		// if the result is also not partial transfer, then some other error happened, return that no data was transfered
		if(res != RW_PARTIAL_TRANSFER ) {
			return 0;	
		}
	
		// if we got here, then partial transfer happened, see how much data we got
		commandShort[4] = NET_CMD_CN_GET_DATA_COUNT;
		commandShort[5] = handle;										
	
		res = acsi_cmd(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);           // send command to host over ACSI

		if(res != E_NORMAL) {                                                               // failed? say that no data was transfered
			return 0;
		}

    	count = getDword(pDmaBuffer);                                                       // read how much data was read
	}
	return count;
}

//-------------------------------------------------------------------------------

// for small reads get the data through the local buffers
DWORD read_small(int16 handle, DWORD countNeeded, BYTE *buffer)
{
    DWORD countDone = 0;
    WORD dataLeft, copyCount;
    
	TConInfo *ci    = &conInfo[handle];								// to shorten the following operations use this pointer 
    dataLeft        = ci->rCount - ci->rStart;	                        // see how many data we have buffered									
    
    while(countNeeded > 0) {

        if(dataLeft == 0) {                                             // no data buffered?
            fillReadBuffer(handle);									    // fill the read buffer with new data
            dataLeft = ci->rCount - ci->rStart;	                        // see how many data we have buffered
            
            if(dataLeft == 0) {                                         // if nothing left, quit and return how many we got
                break;
            }
        }
        
        copyCount = (dataLeft > countNeeded) ? countNeeded : dataLeft;  // do we have more data than we need? Just use only what we need, otherwise use all data left
		memcpy(buffer, &ci->rBuf[ ci->rStart], copyCount);

        buffer      += copyCount;                                       // update pointer to where next data should be stored
        countDone   += copyCount;                                       // add to count of bytes read
        countNeeded -= copyCount;                                       // subtract from count of bytes needed to be read

        ci->rStart  += copyCount;                                       // update pointer to first unused data
        dataLeft    = ci->rCount - ci->rStart;	                        // see how many data we have buffered									
    }

    return countDone;
}

// for big reads use buffered data, then do big data transfer (multiple sectors at once), then finish with buffered data
DWORD read_big(int16 handle, DWORD countNeeded, BYTE *buffer)
{
	TConInfo *ci	    = &conInfo[handle];					        // to shorten the following operations use this pointer 
	WORD dataLeft	    = ci->rCount - ci->rStart;										
    
    DWORD countDone     = 0;
    DWORD dwBuffer      = (DWORD) buffer;
    BYTE bufferIsOdd	= dwBuffer	& 1;
	BYTE dataLeftIsOdd;
    char seekOffset     = 0;	

    // First phase of BIG fread:
    // Use all the possible buffered data, and also make the buffer pointer EVEN, as the ACSI transfer works only on EVEN addresses.
    // This means that if the buffer pointer is EVEN, don't use too much data and don't make it ODD this way;
    // and if the buffer pointer is ODD, then make it EVEN - either use already available buffered data, or read data to buffer.
    
    if(bufferIsOdd) {                                                   // if buffer address is ODD, we need to make it EVEN before the big ACSI transfer starts
        if(dataLeft == 0) {                                             // no data buffered?
            fillReadBuffer(handle);									// fill the read buffer with new data
            dataLeft = ci->rCount - ci->rStart;	                        // see how many data we have buffered
            
            if(dataLeft == 0) {                                         // no data in the file? fail
                return 0;
            }
        }
    
        // ok, so we got some data buffered, now use only ODD number of data, so the buffer pointer after this would be EVEN
        dataLeftIsOdd = dataLeft & 1;

        if(!dataLeftIsOdd) {                                            // remaining buffered data count is EVEN? Use only ODD part
            dataLeft    -= 1;
            seekOffset  = -1;
        }
    } else {                                                            // if buffer address is EVEN, we don't need to fix the buffer to be on EVEN address
        if(dataLeft != 0) {                                             // so use buffered data if there are some (otherwise skip this step)
            dataLeftIsOdd = dataLeft & 1;

            if(dataLeftIsOdd) {                                         // if the data left is ODD, use one byte less - to keep the buffer pointer EVEN
                dataLeft--;
                seekOffset = -1;
            }
        }    
    }

    if(dataLeft != 0) {                                                 // if should copy some remaining buffered data, do it
  		memcpy(buffer, &ci->rBuf[ ci->rStart], dataLeft);               // copy the data
            
        buffer      += dataLeft;                                        // update pointer to where next data should be stored
        countDone   += dataLeft;                                        // add to count of bytes read
        countNeeded -= dataLeft;                                        // subtract from count of bytes needed to be read
    }
    
	ci->rStart = 0;													    // mark that the buffer doesn't contain any data anymore (although it might contain 1 byte)
    ci->rCount = 0;
    //---------------
    if(ci->bytesToRead < countNeeded) {                                 // if we have less data in the file than what the caller requested, update the countNeeded
        countNeeded = ci->bytesToRead;
    }
    
    ci->bytesToRead -= countNeeded;                                     // subtract the amount we will use from host buffer / socket
    
    // Second phase of BIG fread: transfer data by blocks of size 512 bytes, buffer must be EVEN
	while(countNeeded >= 512) {											// while we're not at the ending sector
		WORD sectorCount = countNeeded / 512; 
		
		if(sectorCount > MAXSECTORS) {								    // limit the maximum sectors read in one cycle to MAXSECTORS
			sectorCount = MAXSECTORS;
		}
			
		DWORD bytesCount = ((DWORD) sectorCount) * 512;				    // convert sector count to byte count
			
		DWORD res = readData(handle, buffer, bytesCount, seekOffset);	// try to read the data
			
		countDone	    += res;											// update the bytes read variable
		buffer		    += res;											// update the buffer pointer
		countNeeded     -= res;											// update the count that we still should read
			
		if(res != bytesCount) {										    // if failed to read all the requested data?
			return countDone;										    // return with the count of read data 
		}
	}
    //--------------
        
    // Third phase of BIG fread: if the rest after reading big blocks is not 0, we need to finish it with one last buffered read    
	if(countNeeded != 0) {												
		fillReadBuffer(handle);
			
		DWORD rest = (countNeeded <= ci->rCount) ? countNeeded : ci->rCount;    // see if we have enough data to read the rest, and use which is lower - either what we want to read, or what we can read
			
		memcpy(buffer, &ci->rBuf[ ci->rStart ], rest);				    // copy the data that we have
		ci->rStart	+= rest;										    // and move the pointer further in buffer
		countDone	+= rest;										    // also mark that we've read this rest
	}

    return countDone;                                                   // return how much bytes we've read together
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

