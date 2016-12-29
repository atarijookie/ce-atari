#include <mint/osbind.h> 
#include <mint/linea.h> 
#include <stdio.h>

#include "acsi.h"
#include "translated.h"
#include "gemdos.h"
#include "gemdos_errno.h"
#include "VT52.h"
#include "cookiejar.h"
#include "version.h"
#include "hdd_if.h"
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

BYTE findDevice     (void);
BYTE findDeviceSub  (BYTE CEnotCS);
BYTE getSDcardId    (BYTE fromCEnotCS);
BYTE ce_identify    (BYTE ACSI_id);
BYTE cs_inquiry     (BYTE id);
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

//----------------------------
void scanBusForCE(void)
{
    // detect TOS version and try to automatically choose the interface
    WORD  tosVersion    = Supexec(getTOSversion);
    BYTE  tosMajor      = tosVersion >> 8;
    
    if(tosMajor == 1 || tosMajor == 2) {                // TOS 1.xx or TOS 2.xx -- running on ST
        (void) Cconws("Running on ST, choosing ACSI interface.\n\r");
    
        hdd_if_select(IF_ACSI);
        ifUsed      = IF_ACSI;
    } else if(tosMajor == 4) {                          // TOS 4.xx -- running on Falcon
        (void) Cconws("Running on Falcon, choosing SCSI interface.\n\r");
    
        hdd_if_select(IF_SCSI_FALCON);
        ifUsed      = IF_SCSI_FALCON;
    } else {                                            // TOS 3.xx -- running on TT
        (void) Cconws("Running on TT, plase choose [A]CSI or [S]CSI:");
        
        while(1) {
            BYTE key = Cnecin();
            
            if(key == 'a' || key == 'A') {      // A pressed, choosing ACSI
                (void) Cconws("\n\rACSI selected.\n\r");
                hdd_if_select(IF_ACSI);
                ifUsed      = IF_ACSI;
                break;
            }
            
            if(key == 's' || key == 'S') {      // S pressed, choosing SCSI
                (void) Cconws("\n\rSCSI selected.\n\r");
                hdd_if_select(IF_SCSI_TT);
                ifUsed      = IF_SCSI_TT;
                break;
            }
        }
    }

    //---------------------- 
    // search for device on the ACSI bus
    ceFoundNotManual    = 0;
    deviceID            = findDevice();
    
    //----------------------
    // no device ID? just quit
    if(deviceID == 0xff) {  
        return;
    }
    
    commandLong[0] = (deviceID << 5) | 0x1f;
    //----------------------
    // got the device, time to get more info from it
    getSDinfo();
}

//----------------------------
BYTE findDevice(void)
{
    BYTE newId;

    ceFoundNotManual        = FALSE;            // device not found
    isCEnotCS               = TRUE;             // it's CE
    hdIf.maxRetriesCount    = 0;                // disable retries

    while(1) {
        Clear_home();
        newId = findDeviceSub(TRUE);            // try to find CE
        
        if(newId != 0xff) {                     // found? return it
            commandLong[0]          = (newId << 5) | 0x1f;
            ceFoundNotManual        = TRUE;     // device found (not manually selected)
            isCEnotCS               = TRUE;     // it's CE
            hdIf.maxRetriesCount    = 16;       // enable retries for CE
            return newId;
        }
        
        newId = findDeviceSub(FALSE);           // try to find CS
        
        if(newId != 0xff) {                     // found? return it
            commandLong[0]          = (newId << 5) | 0x1f;
            ceFoundNotManual        = TRUE;     // device found (not manually selected)
            isCEnotCS               = FALSE;    // it's CS
            hdIf.maxRetriesCount    = 0;        // no retries for CS 
            return newId;
        }

        // device not found, user now chooses: quit, retry, manual ID for CosmoSolo
        (void) Cconws("CE and CS not found.\r\n");
        (void) Cconws("Press 'Q' to quit.\r\n");
        (void) Cconws("Press 'R' to retry scan.\r\n");
        (void) Cconws("Press 0-7 to manually select CS ACSI ID\r\n");        
        
        while(1) {
            BYTE key = Cnecin();        

            if(key == 'Q' || key=='q') {                    // quit?
                hdIf.maxRetriesCount = 16;                  // enable retries
                return 0xff;                                // not found
            }
            
            if(key >= '0' && key <= '7') {                  // manually select ID?
                newId                   = key - '0';        // get new ID
                commandLong[0]          = (newId << 5) | 0x1f;
                ceFoundNotManual        = FALSE;            // device not found, but manually selected
                isCEnotCS               = FALSE;            // it's CS
                hdIf.maxRetriesCount    = 0;                // no retries for CS
                return newId;
            }
            
            if(key == 'R' || key=='r') {                    // retry scan?
                break;
            }
        }
    }
}

//----------------------------
BYTE findDeviceSub(BYTE CEnotCS)
{
    BYTE i, res;
    char bfr[2];

    hdIf.maxRetriesCount = 0;           // disable retries - we are expecting that the devices won't answer on every ID
    
    bfr[1] = 0;
    
    if(CEnotCS) {                       // looking for CE
        (void) Cconws("CosmosEx   on ");
    } else {                            // looking for CS
        (void) Cconws("CosmosSolo on ");
    }
    
    switch(ifUsed) {
        case IF_ACSI:           (void) Cconws("ACSI: ");        break;
        case IF_SCSI_TT:        (void) Cconws("TT SCSI: ");     break;
        case IF_SCSI_FALCON:    (void) Cconws("Falcon SCSI: "); break;
    }

    for(i=0; i<8; i++) {
        bfr[0] = i + '0';
        (void) Cconws(bfr); 
          
        if(CEnotCS) {                               // looking for CE
            res = ce_identify(i);                   // try to read the IDENTITY string
        } else {                                    // looking for CS
            res = cs_inquiry(i);                    // try to read the INQUIRY string
        }
  
        if(res == TRUE) {                           // if found the CosmosEx 
            return i;
        }
    }

    (void) Cconws("\r\n");
    return 0xff;                                    // not found
}

//--------------------------------------------------
BYTE ce_identify(BYTE ACSI_id)
{
  BYTE cmd[CMD_LENGTH_SHORT] = {0, 'C', 'E', HOSTMOD_TRANSLATED_DISK, TRAN_CMD_IDENTIFY, 0};
  
  cmd[0] = (ACSI_id << 5);                  // cmd[0] = ACSI_id + TEST UNIT READY (0)   
  memset(rBuffer, 0, 512);                  // clear the buffer 

  hdIfCmdAsUser(ACSI_READ, cmd, CMD_LENGTH_SHORT, rBuffer, 1);   // issue the identify command and check the result 
    
  if(!hdIf.success || hdIf.statusByte != 0) {                   // if failed, return FALSE 
    return FALSE;
  }
    
  if(strncmp((char *) rBuffer, "CosmosEx translated disk", 24) != 0) {      // the identity string doesn't match? 
    return FALSE;
  }
    
  return TRUE;                              // success 
}

//--------------------------------------------------
BYTE cs_inquiry(BYTE id)
{
    BYTE cmd[CMD_LENGTH_SHORT];
    
    memset(cmd, 0, 6);
    cmd[0] = (id << 5) | (SCSI_CMD_INQUIRY & 0x1f);
    cmd[4] = 32;                                                    // count of bytes we want from inquiry command to be returned
    
    hdIfCmdAsUser(ACSI_READ, cmd, CMD_LENGTH_SHORT, rBuffer, 1);    // issue the inquiry command and check the result 
    
    if(!hdIf.success || hdIf.statusByte != OK) {                    // if failed, return FALSE 
        return FALSE;
    }

    if(strncmp(((char *) rBuffer) + 16, "CosmoSolo", 9) != 0) {     // the inquiry string doesn't match? fail
        return FALSE;
    }

    return TRUE;
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
