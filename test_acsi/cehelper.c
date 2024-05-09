#include <mint/osbind.h> 
#include <mint/linea.h> 
#include <stdio.h>

#include "../libacsiscsi/acsi.h"
#include "../libacsiscsi/hdd_if.h"
#include "../libacsiscsi/stdlib.h"
#include "translated.h"
#include "gemdos.h"
#include "gemdos_errno.h"
#include "VT52.h"
#include "version.h"
#include "main.h"

extern uint8_t deviceID, sdCardId;
extern uint8_t isCEnotCS;
extern uint8_t sdCardPresent;
extern uint8_t ceFoundNotManual;
extern uint8_t commandLong [CMD_LENGTH_LONG];
extern uint8_t *readBuffer;
extern uint8_t *writeBuffer;
extern uint8_t *rBuffer, *wBuffer;
extern uint8_t prevCommandFailed;
extern uint8_t ifUsed;

uint8_t getSDcardId    (uint8_t fromCEnotCS);
void hdIfCmdAsUser  (uint8_t readNotWrite, uint8_t *cmd, uint8_t cmdLength, uint8_t *buffer, uint16_t sectorCount);

uint16_t sdErrorCountWrite;
uint16_t sdErrorCountRead;

void getSDinfo(void);

//--------------------------------------------------
uint8_t getSDcardId(uint8_t fromCEnotCS)
{
    if(fromCEnotCS) {           // for CE
        uint8_t cmd[CMD_LENGTH_SHORT] = {0, 'C', 'E', HOSTMOD_TRANSLATED_DISK, TEST_GET_ACSI_IDS, 0};

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
void getSDcardErrorCounters(uint8_t doReset)
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
