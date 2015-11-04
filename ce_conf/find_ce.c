//--------------------------------------------------
#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <support.h>

#include <stdint.h>
#include <stdio.h>

#include "stdlib.h"
#include "acsi.h"
#include "find_ce.h"
#include "global.h"
#include "hdd_if.h"

extern BYTE  deviceID;
extern BYTE  cosmosExNotCosmoSolo;
extern BYTE *pBuffer;

BYTE findCE(BYTE device, BYTE hddIf);
BYTE ce_identify(BYTE id, BYTE hddIf);
BYTE cs_inquiry (BYTE id, BYTE hddIf);

//--------------------------------------------------

BYTE getMachineType(void);
BYTE machine;

//--------------------------------------------------

#define DEVICE_CE     1
#define DEVICE_CS     0     

//--------------------------------------------------

BYTE findDevice(void)
{
    BYTE found = 0;
    BYTE key;

    hdIf.retriesDoneCount = 0;                                  // disable retries - we are expecting that the devices won't answer on every ID
    
    machine = getMachineType();
    
    while(1) {
        if(machine == MACHINE_ST) {                             // for ST
            found = findCE(DEVICE_CE, IF_ACSI);                 // CE on ACSI
        
            if(!found) {
                found = findCE(DEVICE_CS, IF_ACSI);             // CS on ACSI
            }
        }

        if(machine == MACHINE_TT) {                             // for TT
            found = findCE(DEVICE_CE, IF_ACSI);                 // CE on ACSI
        
            if(!found) {
                found = findCE(DEVICE_CE, IF_SCSI_TT);          // CE on SCSI TT
            }

            if(!found) {
                found = findCE(DEVICE_CS, IF_ACSI);             // CS on ACSI
            }
        
            if(!found) {
                found = findCE(DEVICE_CS, IF_SCSI_TT);          // CS on SCSI TT
            }
        }

        if(machine == MACHINE_FALCON) {                         // for Falcon
            found = findCE(DEVICE_CE, IF_SCSI_FALCON);          // CE on SCSI FALCON

            if(!found) {
                found = findCE(DEVICE_CS, IF_SCSI_FALCON);      // CS on SCSI FALCON
            }
        }
        
        if(found) {
            hdIf.retriesDoneCount = 16;                         // enable retries
            return TRUE;
        }
        //---------------------
  		(void) Cconws("\n\rDevice not found.\n\rPress any key to retry or 'Q' to quit.\n\r");
		key = Cnecin();
    
		if(key == 'Q' || key=='q') {
            hdIf.retriesDoneCount = 16;                     // enable retries
			return FALSE;
		}
    }
    
    hdIf.retriesDoneCount = 16;                             // enable retries
    return FALSE;                                           // this should never happen
}

//--------------------------------------------------
BYTE findCE(BYTE device, BYTE hddIf)
{
    BYTE id, res;
    
	(void) Cconws("\n\rLooking for ");
    
    if(device == DEVICE_CE) {                       // looking for CosmosEx?
        (void) Cconws("CosmosEx ");
    } else {                                        // looking for CosmoSolo
        (void) Cconws("CosmoSolo");
    }
    
    (void) Cconws(" on ");
    
    if(hddIf == IF_ACSI) {                          // ACSI?
        (void) Cconws("ACSI: ");
    } else {                                        // SCSI?
        (void) Cconws("SCSI: ");
    }

    hdd_if_select(hddIf);                           // select HDD IF
    
    for(id=0; id<8; id++) {                         // try to talk to all ACSI devices
        Cconout('0' + id);                          // write out BUS ID
    
        if(device == DEVICE_CE) {                   // looking for CosmosEx? 
            res = ce_identify(id, hddIf);           // try to read the IDENTITY string 
        } else {                                    // looking for CosmoSolo?
            res = cs_inquiry (id, hddIf);           // try to read INQUIRY string
        }
  
        if(res == 1) {                              // if found the CosmosEx 
            (void) Cconws(" <-- found!\n\r");
            
            deviceID                = id;           // store the BUS ID of device
            cosmosExNotCosmoSolo    = device;       // store device type

            return TRUE;
        }
    }
  
    return FALSE;
}
//--------------------------------------------------
BYTE ce_identify(BYTE id, BYTE hddIf)
{
    BYTE cmd[] = {0, 'C', 'E', HOSTMOD_CONFIG, CFG_CMD_IDENTIFY, 0};
  
    cmd[0] = (id << 5); 					                    // cmd[0] = ACSI_id + TEST UNIT READY (0)
    memset(pBuffer, 0, 512);              	                    // clear the buffer 
  
    (*hdIf.cmd)(1, cmd, 6, pBuffer, 1);                         // issue the identify command and check the result 
    
    if(!hdIf.success || hdIf.statusByte != OK) {                // if failed, return FALSE 
        return FALSE;
    }
    
    if(strncmp((char *) pBuffer, "CosmosEx config console", 23) != 0) { // the identity string doesn't match? 
        return FALSE;
    }
	
    return TRUE;                                                // success 
}
//--------------------------------------------------
BYTE cs_inquiry(BYTE id, BYTE hddIf)
{
    BYTE cmd[6];
    
    memset(cmd, 0, 6);
    cmd[0] = (id << 5) | (SCSI_CMD_INQUIRY & 0x1f);
    cmd[4] = 32;                                                // count of bytes we want from inquiry command to be returned
    
    (*hdIf.cmd)(1, cmd, 6, pBuffer, 1);	                        // issue the inquiry command and check the result 
    
    if(!hdIf.success || hdIf.statusByte != OK) {                // if failed, return FALSE 
        return FALSE;
    }

    if(strncmp(((char *) pBuffer) + 16, "CosmoSolo", 9) != 0) { // the inquiry string doesn't match? fail
        return FALSE;
    }

	return TRUE;
}
//--------------------------------------------------
BYTE getMachineType(void)
{
    DWORD *cookieJarAddr    = (DWORD *) 0x05A0;
    DWORD *cookieJar        = (DWORD *) *cookieJarAddr;     // get address of cookie jar
    
    if(cookieJar == 0) {                        // no cookie jar? it's an old ST
        return MACHINE_ST;
    }
    
    DWORD cookieKey, cookieValue;
    
    while(1) {                                  // go through the list of cookies
        cookieKey   = *cookieJar++;
        cookieValue = *cookieJar++;
        
        if(cookieKey == 0) {                    // end of cookie list? then cookie not found, it's an ST
            break;
        }
        
        if(cookieKey == 0x5f4d4348) {           // is it _MCH key?
            WORD machine = cookieValue >> 16;
            
            switch(machine) {                   // depending on machine, either it's TT or FALCON
                case 2: return MACHINE_TT;
                case 3: return MACHINE_FALCON;
            }
            
            break;                              // or it's ST
        }
    }

    return MACHINE_ST;                          // it's an ST
}
//--------------------------------------------------

