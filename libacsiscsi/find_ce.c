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

//--------------------------------------------------

uint8_t getMachineType(void);
uint8_t machine;

//--------------------------------------------------
uint8_t findWhatDev;

uint8_t findDevice(uint8_t whatDev)
{
    findWhatDev = whatDev;      // store what device we should be looking for

    uint8_t mode = Super(1);    // this interrogates the current mode of the processor

    // If the processor is in user mode a SUP_USER (0) is returned,
    // otherwise a SUP_SUPER (1) is returned.

    if(mode == 0) { // user mode a SUP_USER (0), call with Supexec()
        return Supexec(findDeviceInSupervisor);
    }

    // SUP_SUPER (1) is returned, running in supervisor, just call the function
    return findDeviceInSupervisor();
}

uint8_t findDeviceInSupervisor(void)
{
    uint8_t deviceId = DEVICE_NOT_FOUND;
    uint8_t key;

    hdIf.maxRetriesCount = 0;                                   // disable retries - we are expecting that the devices won't answer on every ID
    
    machine = getMachineType();
    
    while(1) {
        if(machine == MACHINE_ST) {                             // for ST
            deviceId = findCE(FIND_DEV_CE, IF_ACSI);               // CE on ACSI
        
            if(deviceId == DEVICE_NOT_FOUND) {
                deviceId = findCE(FIND_DEV_CS, IF_ACSI);           // CS on ACSI
            }
        }

        if(machine == MACHINE_TT) {                             // for TT
            deviceId = findCE(FIND_DEV_CE, IF_ACSI);               // CE on ACSI
        
            if(deviceId == DEVICE_NOT_FOUND) {
                deviceId = findCE(FIND_DEV_CE, IF_SCSI_TT);        // CE on SCSI TT
            }

            if(deviceId == DEVICE_NOT_FOUND) {
                deviceId = findCE(FIND_DEV_CS, IF_ACSI);           // CS on ACSI
            }
        
            if(deviceId == DEVICE_NOT_FOUND) {
                deviceId = findCE(FIND_DEV_CS, IF_SCSI_TT);        // CS on SCSI TT
            }
        }

        if(machine == MACHINE_FALCON) {                         // for Falcon
            deviceId = findCE(FIND_DEV_CE, IF_SCSI_FALCON);        // CE on SCSI FALCON

            if(deviceId == DEVICE_NOT_FOUND) {
                deviceId = findCE(FIND_DEV_CS, IF_SCSI_FALCON);    // CS on SCSI FALCON
            }
        }
        
        if(deviceId != DEVICE_NOT_FOUND) {      // device found?
            hdIf.maxRetriesCount = 16;          // enable retries
            return deviceId;
        }
        //---------------------
  		(void) Cconws("\n\rDevice not found.\n\rPress any key to retry or 'Q' to quit.\n\r");
		key = Cnecin();
    
		if(key == 'Q' || key=='q') {
            hdIf.maxRetriesCount = 16;                      // enable retries
			return DEVICE_NOT_FOUND;
		}
    }
    
    hdIf.maxRetriesCount = 16;                              // enable retries
    return DEVICE_NOT_FOUND;                                // this should never happen
}

//--------------------------------------------------
uint8_t findCE(uint8_t device, uint8_t hddIf)
{
    uint8_t id, res;

    uint8_t findDevCE = (findWhatDev & FIND_DEV_CE);    // should be looking for CE?
    uint8_t findDevCS = (findWhatDev & FIND_DEV_CS);    // should be looking for CS?

    if( (!findDevCE && device == FIND_DEV_CE) ||    // we shouldn't look for CE, but we're told to look for CE? pretend not found
        (!findDevCS && device == FIND_DEV_CS)) {    // we shouldn't look for CS, but we're told to look for CS? pretend not found
            return DEVICE_NOT_FOUND;
    }

	(void) Cconws("\n\rLooking for ");
    
    if(device == FIND_DEV_CE) {                     // looking for CosmosEx?
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
    
        if(device == FIND_DEV_CE) {                 // looking for CosmosEx? 
            res = ce_identify(id, hddIf);           // try to read the IDENTITY string 
        } else {                                    // looking for CosmoSolo?
            res = cs_inquiry (id, hddIf);           // try to read INQUIRY string
        }
  
        if(res == 1) {                              // if found the CosmosEx 
            (void) Cconws(" <-- found!\n\r");
            
            if(device == FIND_DEV_CS) {             // if it's CS, set highest bit
                id = id | 0x80;
            }
            return id;
        }
    }
  
    return DEVICE_NOT_FOUND;
}
//--------------------------------------------------
uint8_t ce_identify(uint8_t id, uint8_t hddIf)
{
    uint8_t cmd[] = {0, 'C', 'E', HOSTMOD_TRANSLATED_DISK, TRAN_CMD_IDENTIFY, 0};
    uint8_t buffer[514];
    uint32_t uBuffer = (uint32_t) buffer;               // pointer to unsigned integer
    uBuffer = (uBuffer & 1) ? (uBuffer + 1) : uBuffer;  // odd bit set? increment, else keep as is
    uint8_t* pBuffer = (uint8_t*) uBuffer;              // unsigned integer to pointer

    cmd[0] = (id << 5); 					        // cmd[0] = ACSI_id + TEST UNIT READY (0)
    memset(pBuffer, 0, 512);              	        // clear the buffer

    (*hdIf.cmd)(1, cmd, 6, pBuffer, 1);             // issue the identify command and check the result

	if(!hdIf.success || hdIf.statusByte != OK) {    // if failed, return FALSE
		return 0;
	}

	if(strncmp((char *) pBuffer, "CosmosEx translated disk", 24) != 0) {     // the identity string doesn't match?
		return 0;
	}

    return TRUE;                                    // success
}
//--------------------------------------------------
uint8_t cs_inquiry(uint8_t id, uint8_t hddIf)
{
    uint8_t cmd[6];
    uint8_t buffer[514];
    uint32_t uBuffer = (uint32_t) buffer;               // pointer to unsigned integer
    uBuffer = (uBuffer & 1) ? (uBuffer + 1) : uBuffer;  // odd bit set? increment, else keep as is
    uint8_t* pBuffer = (uint8_t*) uBuffer;              // unsigned integer to pointer

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
