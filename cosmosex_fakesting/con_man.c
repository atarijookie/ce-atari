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

//---------------------
// ACSI / CosmosEx stuff
#include "acsi.h"
#include "ce_commands.h"
#include "stdlib.h"

//--------------------------------------

extern  uint32  localIP;

TConInfo conInfo[MAX_HANDLE];                   // this holds info about each connection

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
    if(!handle_valid(handle)) {         // we don't have this handle? fail
        return (CIB *) NULL;
    }

    update_con_info();                  // update connections info structs (max once per 100 ms)

	return &conInfo[handle].cib;        // return pointer to correct CIB
}

int16 CNbyte_count (int16 handle)
{
    if(!handle_valid(handle)) {         // we don't have this handle? fail
        return E_BADHANDLE;
    }

    update_con_info();                  // update connections info structs (max once per 100 ms)

    if(conInfo[handle].bytesToRead > 0x7FFF) {  // if we can now receive more than 0x7FFF bytes, return just 0x7FFF, because otherwise it would look like negative error
        return 0x7FFF;
    } 
    
    // if have less than 0x7FFF, just return the value             
    return conInfo[handle].bytesToRead;
}

//-------------------------------------
// data retrieval functions
int16 CNget_char (int16 handle)
{   
    if(!handle_valid(handle)) {         // we don't have this handle? fail
        return E_BADHANDLE;
    }

    update_con_info();                  // update connections info structs (max once per 100 ms)

    if(conInfo[handle].bytesToRead == 0) {      // no data?
        return E_NODATA;
    }
    
    
    
    

    return 0;
}

NDB *CNget_NDB (int16 handle)
{
    if(!handle_valid(handle)) {         // we don't have this handle? fail
        return (NDB *) NULL;
    }

    update_con_info();                  // update connections info structs (max once per 100 ms)

    if(conInfo[handle].bytesToRead == 0) {      // no data?
        return (NDB *) E_NODATA;
    }

    
    
    
    return 0;
}

int16 CNget_block (int16 handle, void *buffer, int16 length)
{
    if(!handle_valid(handle)) {         // we don't have this handle? fail
        return E_BADHANDLE;
    }

    update_con_info();                  // update connections info structs (max once per 100 ms)

    if(conInfo[handle].bytesToRead == 0) {      // no data?
        return E_NODATA;
    }

    
    
    
    return 0;
}

int16 CNgets (int16 handle, char *buffer, int16 length, char delimiter)
{
    if(!handle_valid(handle)) {         // we don't have this handle? fail
        return E_BADHANDLE;
    }

    update_con_info();                  // update connections info structs (max once per 100 ms)

    if(conInfo[handle].bytesToRead == 0) {      // no data?
        return E_NODATA;
    }




    
	return 0;
}

//--------------------------------------
// helper functions

void structs_init(void)
{
    int i;
    
    for(i=0; i<MAX_HANDLE; i++) {
        memset(&conInfo[i].cib, 0, sizeof(CIB));
    }
}

int handle_valid(int16 h)
{
    if(h < 0 && h >= MAX_HANDLE) {                                          // handle out of range?
        return FALSE;
    }
    
    if(conInfo[h].cib.address.rhost == 0 && conInfo[h].cib.address.rport == 0) {          // if connection not open 
        return FALSE;
    }
    
    return TRUE;
}

void update_con_info(void)
{
	DWORD res;
	static DWORD lastUpdate = 0;
	DWORD now = *HZ_200;

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
    BYTE    *pConnStatus        = (BYTE *)  (pDmaBuffer + 128);                 // offset 128: 32 * 1 bytes - connection status
    DWORD   *pBytesToReadIcmp   = (DWORD *) (pDmaBuffer + 160);                 // offset 160:  1 * 1 DWORD - bytes that can be read from ICMP socket(s)
    
    for(i=0; i<MAX_HANDLE; i++) {                                               // retrieve all the data and fill the variables
        conInfo[i].bytesToRead          = (DWORD)   pBytesToRead[i];
        conInfo[i].tcpConnectionState   = (BYTE)    pConnStatus[i];
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

	if(res != OK) {                                 // if failed, return FALSE 
		return E_LOSTCARRIER;
	}

    if(handleIsFromCE(res)) {                       // if it's CE handle
        int stHandle = handleCEtoAtari(res);        // convert it to ST handle

        // store info to CIB and CAB structures
        if(tcpNotUdp) {
            conInfo[stHandle].cib.protocol  = TCP;
        } else {
            conInfo[stHandle].cib.protocol  = UDP;
        }
        
        conInfo[stHandle].cib.status            = 0;        // 0 means normal
        conInfo[stHandle].cib.address.rport     = rem_port; // Remote machine port
        conInfo[stHandle].cib.address.rhost     = rem_host; // Remote machine IP address
        conInfo[stHandle].cib.address.lport     = 0;        // Local  machine port
        conInfo[stHandle].cib.address.lhost     = localIP;  // Local  machine IP address
        
        return stHandle;                            // return the new handle
    } 

    // it's not a CE handle
    return extendByteToWord(res);                   // extend the BYTE error code to WORD
}

//-------------------------------------------------------------------------------

int16 connection_close(int tcpNotUdp, int16 handle, int16 mode, int16 *result)
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
    pBfr = storeWord    (pBfr, mode);

    // send it to host
    BYTE res = acsi_cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);

	if(res != OK) {                                 // if failed, return FALSE 
		return E_LOSTCARRIER;
	}

    memset(&conInfo[handle].cib, 0, sizeof(CIB));          // clear the CIB structure
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
    
    if((length % 512) == 0) {                       // if the number of bytes is not multiple of 512, then we need to send one sector more
        sectorCount++;
    }
    
    // send it to host
    BYTE res = acsi_cmd(ACSI_WRITE, commandLong, CMD_LENGTH_LONG, pBfr, 1);

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
    
    res = acsi_cmd(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);

	if(res != OK) {                                                     // if failed, return FALSE 
		return E_CANTRESOLVE;
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


