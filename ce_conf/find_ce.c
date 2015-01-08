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

extern BYTE  busTypeACSInotSCSI;
extern BYTE  deviceID;
extern BYTE  cosmosExNotCosmoSolo;
extern BYTE *pBuffer;

BYTE findCE(BYTE devAndBus);
BYTE ce_identify(BYTE id, BYTE acsiNotScsi);
BYTE cs_inquiry (BYTE id, BYTE acsiNotScsi);

//--------------------------------------------------

BYTE getMachineType(void);

#define MACHINE_ST      0
#define MACHINE_TT      2
#define MACHINE_FALCON  3

//--------------------------------------------------

#define FIND_CE_ACSI    0
#define FIND_CE_SCSI    1
#define FIND_CS_ACSI    2
#define FIND_CS_SCSI    3

//--------------------------------------------------

BYTE findDevice(void)
{
    int  devAndBus;
    BYTE found = 0;
    BYTE key;

    BYTE machine = getMachineType();
    
    while(1) {
        for(devAndBus = FIND_CE_ACSI; devAndBus <= FIND_CS_SCSI; devAndBus++) { // go through all the buses and devices
            
            // if it's ST, don't check SCSI bus
            if(machine == MACHINE_ST        && (devAndBus == FIND_CE_SCSI || devAndBus == FIND_CS_SCSI)) {
                continue;
            }
        
            // if it's Falcon, don't check ACSI bus
            if(machine == MACHINE_FALCON    && (devAndBus == FIND_CE_ACSI || devAndBus == FIND_CS_ACSI)) {
                continue;
            }
        
            found = findCE(devAndBus);          // try to find the device
        
            if(found) {
                return TRUE;
            }
        }
        
        //---------------------
  		(void) Cconws("\n\rDevice not found.\n\rPress any key to retry or 'Q' to quit.\n\r");
		key = Cnecin();
    
		if(key == 'Q' || key=='q') {
			return FALSE;
		}
    }
    
    return FALSE;                   // this should never happen
}

//--------------------------------------------------
BYTE findCE(BYTE devAndBus)
{
    BYTE id, res;
    BYTE cosmosExNotSolo;
    BYTE acsiNotScsi;
    
	(void) Cconws("\n\rLooking for ");
    
    if(devAndBus == FIND_CE_ACSI || devAndBus == FIND_CE_SCSI) {        // looking for CosmosEx?
        (void) Cconws("CosmosEx ");
        cosmosExNotSolo = TRUE;
    } else {                                                            // looking for CosmoSolo
        (void) Cconws("CosmoSolo");
        cosmosExNotSolo = FALSE;
    }
    
    (void) Cconws(" on ");
    
    if(devAndBus == FIND_CE_ACSI || devAndBus == FIND_CS_ACSI) {        // ACSI?
        (void) Cconws("ACSI: ");
        acsiNotScsi = TRUE;
    } else {                                                            // SCSI?
        (void) Cconws("SCSI: ");
        acsiNotScsi = FALSE;
    }

    for(id=0; id<8; id++) {                     // try to talk to all ACSI devices
        Cconout('0' + id);                      // write out BUS ID
    
        if(cosmosExNotSolo) {                   // looking for CosmosEx? 
            res = ce_identify(id, acsiNotScsi); // try to read the IDENTITY string 
        } else {                                // looking for CosmoSolo?
            res = cs_inquiry (id, acsiNotScsi); // try to read INQUIRY string
        }
  
        if(res == 1) {                                  // if found the CosmosEx 
            (void) Cconws(" <-- found!\n\r");
            
            deviceID                = id;               // store the BUS ID of device
            busTypeACSInotSCSI      = acsiNotScsi;      // store BUS TYPE
            cosmosExNotCosmoSolo    = cosmosExNotSolo;  // store device type

            return TRUE;
        }
    }
  
    return FALSE;
}
//--------------------------------------------------
BYTE ce_identify(BYTE id, BYTE acsiNotScsi)
{
    WORD res;
    BYTE cmd[] = {0, 'C', 'E', HOSTMOD_CONFIG, CFG_CMD_IDENTIFY, 0};
  
    cmd[0] = (id << 5); 					    // cmd[0] = ACSI_id + TEST UNIT READY (0)
    memset(pBuffer, 0, 512);              	    // clear the buffer 
  
    if(acsiNotScsi) {                           // for ACSI
        res = acsi_cmd(1, cmd, 6, pBuffer, 1);	// issue the identify command and check the result 
    } else {                                    // for SCSI
        res = ACSIERROR;                        // TODO: for now only error
    }
    
    if(res != OK) {                        	    // if failed, return FALSE 
        return FALSE;
    }
    
    if(strncmp((char *) pBuffer, "CosmosEx config console", 23) != 0) {		// the identity string doesn't match? 
        return FALSE;
    }
	
    return TRUE;                                // success 
}
//--------------------------------------------------
BYTE cs_inquiry(BYTE id, BYTE acsiNotScsi)
{
	int res;
    BYTE cmd[6];
    
    memset(cmd, 0, 6);
    cmd[0] = (id << 5) | (SCSI_CMD_INQUIRY & 0x1f);
    cmd[4] = 32;                                // count of bytes we want from inquiry command to be returned
    
    if(acsiNotScsi) {                           // for ACSI
        res = acsi_cmd(1, cmd, 6, pBuffer, 1);	// issue the identify command and check the result 
    } else {                                    // for SCSI
        res = ACSIERROR;                        // TODO: for now only error
    }
    
    if(res != OK) {                        	    // if failed, return FALSE 
        return FALSE;
    }

    if(strncmp(((char *) pBuffer) + 16, "CosmoSolo", 9) != 0) {     // the inquiry string doesn't match? fail
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

