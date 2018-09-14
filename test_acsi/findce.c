#include <mint/osbind.h> 
#include <mint/linea.h> 
#include <stdio.h>

#include "translated.h"
#include "gemdos.h"
#include "gemdos_errno.h"
#include "VT52.h"
#include "cookiejar.h"
#include "version.h"

#include "../ce_hdd_if/stdlib.h"
#include "../ce_hdd_if/hdd_if.h"
#include "stdlib.h"

extern BYTE deviceID, sdCardId;
extern BYTE isCEnotCS;
extern BYTE sdCardPresent;
extern BYTE ceFoundNotManual;
extern BYTE commandLong [CMD_LENGTH_LONG ];
extern BYTE *readBuffer;
extern BYTE *writeBuffer;
extern BYTE *rBuffer, *wBuffer;
extern BYTE prevCommandFailed;
extern BYTE ifUsed;

BYTE getSDcardId    (BYTE fromCEnotCS);
void hdIfCmdAsUser  (BYTE readNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount);

WORD sdErrorCountWrite;
WORD sdErrorCountRead;

void getSDinfo(void);

//----------------------------
WORD getTOSversion(void)
{
    // detect TOS version and try to automatically choose the interface
    BYTE  *pSysBase     = (BYTE *) 0x000004F2;
    BYTE  *ppSysBase    = (BYTE *)  ((DWORD )  *pSysBase);                      // get pointer to TOS address
    WORD  tosVersion    = (WORD  ) *(( WORD *) (ppSysBase + 2));                // TOS +2: TOS version
    return tosVersion;
}

//--------------------------------------------------
BYTE getSDcardId(BYTE fromCEnotCS)
{
    if(fromCEnotCS) {           // for CE
        BYTE cmd[CMD_LENGTH_SHORT] = {0, 'C', 'E', HOSTMOD_TRANSLATED_DISK, TEST_GET_ACSI_IDS, 0};

        cmd[0] = (deviceID << 5);                                       // cmd[0] = CE deviceID + TEST UNIT READY (0)   
        memset(rBuffer, 0, 512);                                        // clear the buffer 

        hdIfCmdAsUser(ACSI_READ, cmd, CMD_LENGTH_SHORT, rBuffer, 1);
    } else {                    // for CS
        commandLong[0] = (sdCardId << 5) | 0x1f;                        // SD card device ID
        commandLong[3] = 'S';                                           // for CS
        commandLong[5] = TEST_GET_ACSI_IDS;
        commandLong[6] = 0;                                             // don't reset SD error counters
        
        hdIfCmdAsUser(ACSI_READ, commandLong, CMD_LENGTH_LONG, rBuffer, 1);        
    }
    
    if(!hdIf.success || hdIf.statusByte != 0) {                         // if command failed, return -1 (0xff)
        return 0xff;
    }
    
    int i;
    for(i=0; i<8; i++) {                        // go through ACSI IDs
        if(rBuffer[i] == DEVTYPE_SD) {          // if found SD card, good!
            if(!fromCEnotCS) {                  // if data came from CS, then byte 10 contains if the card is init
                sdCardPresent = rBuffer[10];
            }
   
            return i;                           // return ID of SD card
        }
    }
    
    return 0xff;                                // SD card ACSI ID not found
}

//--------------------------------------------------
void getSDcardErrorCounters(BYTE doReset)
{
    // init counters - we might not be able to get them
    sdErrorCountWrite   = 0;
    sdErrorCountRead    = 0;

    if(sdCardId == 0xff) {                      // if the SD card ID is not configured, don't do anything
        return;
    }

    commandLong[0] = (sdCardId << 5) | 0x1f;    // SD card device ID
    commandLong[3] = 'S';                       // for CS
    commandLong[5] = TEST_GET_ACSI_IDS;

    if(doReset) {                               // do reset?
        commandLong[6] = 'R';
    } else {                                    // don't do reset?
        commandLong[6] = 0;
    }

    hdIfCmdAsUser(ACSI_READ, commandLong, CMD_LENGTH_LONG, rBuffer, 1);        

    if(!hdIf.success || hdIf.statusByte != 0) {                         // if command failed, return -1 (0xff)
        return;
    }

    sdErrorCountWrite   = (rBuffer[11] << 8) | rBuffer[12];
    sdErrorCountRead    = (rBuffer[13] << 8) | rBuffer[14];
}

//----------------------------------------------------
void getSDinfo(void)
{
    if(isCEnotCS) {                             // if it's CE
        sdCardId = getSDcardId(TRUE);           // get SD card ID from CE
        
        if(sdCardId != 0xff) {                  // if the SD card ID is configured
            sdCardId = getSDcardId(FALSE);      // now use that SD card ID to talk to CS and get if card is present
        } else {                                // SD card ID not configured? SD card not present
            sdCardPresent = FALSE;
        }
    } else {                                    // if it's CS
        sdCardId = deviceID;                    // the SD card ID is the same as device ID
        getSDcardId(FALSE);                     // now use that SD card ID to talk to CS and get if card is present
    }
}
