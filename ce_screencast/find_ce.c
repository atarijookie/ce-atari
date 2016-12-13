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
#include "hdd_if.h"
#include "translated.h"

extern BYTE  deviceID;
extern BYTE  cosmosExNotCosmoSolo;
extern BYTE *pDmaBuffer;

BYTE findCE(BYTE hddIf);
BYTE ce_identify(BYTE id, BYTE hddIf);

//--------------------------------------------------

BYTE getMachineType(void);
BYTE machine;                //general machine type (ST(e)/TT/Falcon)
extern volatile DWORD machineconfig; //explicit subsystem information

//--------------------------------------------------

BYTE findDevice(void)
{
    BYTE found = 0;
    BYTE key;

    hdIf.maxRetriesCount = 0;                           // disable retries - we are expecting that the devices won't answer on every ID
    
    machine = getMachineType();
    
    while(1) {
        if(machine == MACHINE_ST) {                     // for ST
            found = findCE(IF_ACSI);                    // CE on ACSI
        }

        if(machine == MACHINE_TT) {                     // for TT
            found = findCE(IF_ACSI);                    // CE on ACSI
        
            if(!found) {
                found = findCE(IF_SCSI_TT);             // CE on SCSI TT
            }
        }

        if(machine == MACHINE_FALCON) {                 // for Falcon
            found = findCE(IF_SCSI_FALCON);             // CE on SCSI FALCON
        }
        
        if(found) {
            hdIf.maxRetriesCount = 16;                  // enable retries
            return TRUE;
        }
        //---------------------
  		(void) Cconws("\n\rDevice not found.\n\rPress any key to retry or 'Q' to quit.\n\r");
		key = Cnecin();
    
		if(key == 'Q' || key=='q') {
            hdIf.maxRetriesCount = 16;                  // enable retries
			return FALSE;
		}
    }
    
    hdIf.maxRetriesCount = 16;                          // enable retries
    return FALSE;                                       // this should never happen
}

//--------------------------------------------------
BYTE findCE(BYTE hddIf)
{
    BYTE id, res;
    
	(void) Cconws("\n\rLooking for CosmosEx on ");

    if(hddIf == IF_ACSI) {                              // ACSI?
        (void) Cconws("ACSI: ");
    } else {                                            // SCSI?
        (void) Cconws("SCSI: ");
    }

    hdd_if_select(hddIf);                               // select HDD IF

    for(id=0; id<8; id++) {                             // try to talk to all ACSI devices
        Cconout('0' + id);                              // write out BUS ID
    
        res = ce_identify(id, hddIf);                   // try to read the IDENTITY string 
  
        if(res == 1) {                                  // if found the CosmosEx 
            (void) Cconws(" <-- found!\n\r");
            
            deviceID                = id;               // store the BUS ID of device
            return TRUE;
        }
    }
  
    return FALSE;
}
//--------------------------------------------------
BYTE ce_identify(BYTE id, BYTE hddIf)
{
    BYTE cmd[] = {0, 'C', 'E', HOSTMOD_TRANSLATED_DISK, TRAN_CMD_IDENTIFY, 0};
  
    cmd[0] = (id << 5); 					        // cmd[0] = ACSI_id + TEST UNIT READY (0)
    memset(pDmaBuffer, 0, 512);              	    // clear the buffer 
  
    (*hdIf.cmd)(1, cmd, 6, pDmaBuffer, 1);          // issue the identify command and check the result 
    
	if(!hdIf.success || hdIf.statusByte != OK) {    // if failed, return FALSE
		return 0;
	}

	if(strncmp((char *) pDmaBuffer, "CosmosEx translated disk", 24) != 0) {     // the identity string doesn't match?
		return 0;
	}
	
    return TRUE;                                    // success 
}
//--------------------------------------------------
BYTE getMachineType(void)
{
    DWORD *cookieJarAddr    = (DWORD *) 0x05A0;
    DWORD *cookieJar        = (DWORD *) *cookieJarAddr;     // get address of cookie jar

    machine_check(); 			//check capabilities on hardware level
    
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
                case 1:
					//STE has $fff820d 
                	machineconfig|=MACHINECONFIG_HAS_STE_SCRADR_LOWBYTE;
					return MACHINE_ST;
                case 2: 
					//TT also has $fff820d 
                	machineconfig|=MACHINECONFIG_HAS_STE_SCRADR_LOWBYTE;
                	machineconfig|=MACHINECONFIG_HAS_TT_VIDEO;
					return MACHINE_TT;
                case 3: 
					//F030 also has $fff820d 
                	machineconfig|=MACHINECONFIG_HAS_STE_SCRADR_LOWBYTE;
                	machineconfig|=MACHINECONFIG_HAS_VIDEL;
					return MACHINE_FALCON;
            }
            
            break;                              // or it's ST
        }
    }

    return MACHINE_ST;                          // it's an ST
}
//--------------------------------------------------

