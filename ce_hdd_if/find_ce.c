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
#include "hdd_if_lowlevel.h"
#include "translated.h"

//--------------------------------------------------
void getMachineType(void);
BYTE machine;

// following static vars are used for passing arguments from user-mode findDevice to supervisor-mode findDevice()
static BYTE __whichIF, __whichDevType, __devTypeFound, __mode;

StatusDisplayer statusDisplayer;

void setStatusDisplayer(void *func)
{
    statusDisplayer = (StatusDisplayer) func;
}

//--------------------------------------------------
// do a generic SCSI INQUIRY command, return TRUE on success
BYTE __id;

static BYTE __scsi_inquiry(void)
{
    #define SCSI_CMD_INQUIRY    0x12

    BYTE cmd[6] = {SCSI_CMD_INQUIRY, 0, 0, 0, 32, 0};
    cmd[0] = (__id << 5) | SCSI_CMD_INQUIRY;

    WORD dmaBuffer[256];                            // declare as WORD buffer to force WORD alignment
    BYTE *pDmaBuffer = (BYTE *)dmaBuffer;

    (*hdIf.cmd)(1, cmd, 6, pDmaBuffer, 1);          // issue the identify command and check the result

    if(!hdIf.success || hdIf.statusByte != OK) {    // if failed completely, return DEV_NONE
        return DEV_NONE;
    }

    if(strncmp(((char *) pDmaBuffer) + 16, "CosmoSolo", 9) == 0) { // the inquiry string matches CosmoSolo, return DEV_CS
        return DEV_CS;
    }

    if(strncmp(((char *) pDmaBuffer) + 16, "CosmosEx", 8) == 0) {  // the inquiry string matches CosmosEx, return DEV_CE
        return DEV_CE;
    }

    return DEV_OTHER;                               // inquiry success, but it's somehting else than CE and CS
}

static BYTE scsi_inquiry(BYTE id)
{
    __id = id;

    if(__mode == SUP_USER) {              // If the processor is in user mode - SUP_USER
        return Supexec(__scsi_inquiry); // call through Supexec()
    } else {                            // If the processor is in supervisor mode - SUP_SUPER
        return __scsi_inquiry();        // call directly
    }
}

//--------------------------------------------------
static BYTE __ce_identify(void)
{
    BYTE cmd[] = {0, 'C', 'E', HOSTMOD_TRANSLATED_DISK, TRAN_CMD_IDENTIFY, 0};

    WORD dmaBuffer[256];                            // declare as WORD buffer to force WORD alignment
    BYTE *pDmaBuffer = (BYTE *)dmaBuffer;

    cmd[0] = (__id << 5);                           // cmd[0] = ACSI_id + TEST UNIT READY (0)
    memset(pDmaBuffer, 0, 512);                     // clear the buffer

    (*hdIf.cmd)(1, cmd, 6, pDmaBuffer, 1);          // issue the identify command and check the result

    if(!hdIf.success || hdIf.statusByte != OK) {    // if failed, return FALSE
        return FALSE;
    }

    if(strncmp((char *) pDmaBuffer, "CosmosEx translated disk", 24) != 0) {     // the identity string doesn't match?
        return FALSE;
    }

    return TRUE;                                    // success
}

static BYTE ce_identify(BYTE id)
{
    __id = id;

    if(__mode == SUP_USER) {              // If the processor is in user mode - SUP_USER
        return Supexec(__ce_identify);  // call through Supexec()
    } else {                            // If the processor is in supervisor mode - SUP_SUPER
        return __ce_identify();         // call directly
    }
}

const char *hddIfToString(BYTE hddIf)
{
    switch(hddIf) {
        case IF_ACSI:           return "ACSI";

        case IF_SCSI_TT:
        case IF_SCSI_FALCON:    return "SCSI";

        case IF_CART:           return "CART";

        default:                return "????";
    }
}

void showFoundMessageThroughDisplayer(BYTE hddIf, BYTE id)
{
    if(!statusDisplayer) {
        return;
    }

    char tmp[20];
    strcpy(tmp, "Found on ");
    strcat(tmp, hddIfToString(hddIf));
    strcat(tmp, " X");          // placeholder for ID

    int len = strlen(tmp);      // get length
    tmp[len - 1] = '0' + id;    // store ID at the end

    (*statusDisplayer)(tmp);
}

void showSearchMessageThroughDisplayer(BYTE hddIf, BYTE id)
{
    if(!statusDisplayer) {
        return;
    }

    char tmp[20];
    strcpy(tmp, hddIfToString(hddIf));
    strcat(tmp, " X");          // placeholder for ID

    int len = strlen(tmp);      // get length
    tmp[len - 1] = '0' + id;    // store ID at the end

    (*statusDisplayer)(tmp);
}

//--------------------------------------------------
// whichDevType -- which dev type we want to find DEV_CE, DEV_CS, DEV_OTHER
// returns device ID (0 - 7), or -1 (0xff) if not found
BYTE findDeviceOnSingleIF(BYTE hddIf, BYTE whichDevType)
{
    BYTE id;
    BYTE devTypeFound;

    if(!statusDisplayer) {
        (void) Cconws("\n\rLooking for device on ");
        (void) Cconws(hddIfToString(hddIf));
        (void) Cconws(": ");
    }

    hdd_if_select(hddIf);           // select HDD IF

    for(id=0; id<8; id++) {         // try to talk to all ACSI devices
        if(!statusDisplayer) {
            Cconout('0' + id);          // write out BUS ID
        } else {
            showSearchMessageThroughDisplayer(hddIf, id);
        }

        devTypeFound = scsi_inquiry(id);    // do generic SCSI INQUIRY to find out if this device lives or not
        if(devTypeFound == DEV_NONE) {      // device not alive, skip further checks
            continue;
        }

        if(whichDevType & DEV_CE) {         // should be looking for CE?
            BYTE res = ce_identify(id);    // try to read the IDENTITY string

            if(res == TRUE) {               // if found
                if(!statusDisplayer) {
                    (void) Cconws(" <-- found CE\n\r");
                } else {
                    showFoundMessageThroughDisplayer(hddIf, id);
                }

                __devTypeFound = DEV_CE;    // found this dev type
                return id;                  // return device ID (0 - 7)
            }
        }

        if(whichDevType & DEV_CS) {         // should be looking for CS?
            if(devTypeFound == DEV_CS) {    // if found
                if(!statusDisplayer) {
                    (void) Cconws(" <-- found CS\n\r");
                } else {
                    showFoundMessageThroughDisplayer(hddIf, id);
                }

                __devTypeFound = DEV_CS;    // found this dev type
                return id;                  // return device ID (0 - 7)
            }
        }

        if(whichDevType & DEV_OTHER) {      // should be looking for other device types? if it came here, it's not CE and not CS, so it's OTHER
            if(!statusDisplayer) {
                (void) Cconws(" <-- found OTHER\n\r");
            } else {
                showFoundMessageThroughDisplayer(hddIf, id);
            }

            __devTypeFound = DEV_OTHER;     // found this dev type
            return id;
        }
    }

    return DEVICE_NOT_FOUND;        // device not found
}
//--------------------------------------------------
void getMachineType(void)
{
    DWORD *cookieJarAddr    = (DWORD *) 0x05A0;
    DWORD *cookieJar        = (DWORD *) *cookieJarAddr;     // get address of cookie jar

    if(cookieJar == 0) {                        // no cookie jar? it's an old ST
        machine = MACHINE_ST;
        return;
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
                case 2: machine = MACHINE_TT;     return;
                case 3: machine = MACHINE_FALCON; return;
            }

            break;                              // or it's ST
        }
    }

    machine = MACHINE_ST;                       // it's an ST
}
//--------------------------------------------------
BYTE __findDevice(void)
{
    BYTE deviceId = 0;
    BYTE key;
    int i;

    hdIf.maxRetriesCount = 0;                           // disable retries - we are expecting that the devices won't answer on every ID

    // based on machine type determine which IF should we search through -- all allowed on running machine
    if(__mode == SUP_USER) {            // If the processor is in user mode - SUP_USER
        Supexec(getMachineType); // call through Supexec()
    } else {                            // If the processor is in supervisor mode - SUP_SUPER
        getMachineType();        // call directly
    }

    BYTE searchIF = IF_NONE;

    switch(machine) {
        case MACHINE_ST:        searchIF =  IF_ACSI ;               break;  // IF_CART disabled 
        //case MACHINE_ST:        searchIF = (IF_ACSI | IF_CART);   break;  // IF_CART enabled
        case MACHINE_TT:        searchIF = (IF_ACSI | IF_SCSI_TT);  break;
        case MACHINE_FALCON:    searchIF = (IF_SCSI_FALCON);        break;
    }

    if(__whichIF != 0) {                            // if __whichIF is specified, use it as mask to narrow used IFs. E.g. for ST we can use ACSI and CART, but with __whichIF set to CART we will search only on CART.
        searchIF = searchIF & __whichIF;
    }

    if(__whichDevType == 0) {                       // if which device type is not specified, look for CosmosEx only
        __whichDevType = DEV_CE;
    }

    while(1) {
        deviceId = DEVICE_NOT_FOUND;                // init to NOT FOUND

        for(i=0; i<8; i++) {                        // go through all the search IFs, and search for device on IF if it's enabled
            BYTE oneIF = searchIF & (1 << i);       // get only one IF
            if(oneIF) {                             // if this IF is enabled, find device on it
                deviceId = findDeviceOnSingleIF(oneIF, __whichDevType);

                if(deviceId != DEVICE_NOT_FOUND) {  // device found? return that ID
                    hdIf.maxRetriesCount = 16;      // enable retries
                    return deviceId;
                }
            }
        }

        if(!statusDisplayer) {
            (void) Cconws("\n\rDevice not found.\n\rPress any key to retry or 'Q' to quit.\n\r");
            key = Cnecin();
        } else {
            (*statusDisplayer)("Device not found.");
            key = 'Q';
            sleep(1);
        }

        if(key == 'Q' || key=='q') {
            hdIf.maxRetriesCount = 16;                  // enable retries
            return DEVICE_NOT_FOUND;
        }
    }

    hdIf.maxRetriesCount = 16;                          // enable retries
    return DEVICE_NOT_FOUND;                            // this should never happen
}
//--------------------------------------------------
// If you use findDevice(0, 0), if will look for CE only on all available buses on current machine.
// whichIF -- which IF we want to specifically search through; can be: IF_ACSI, IF_SCSI_TT, IF_SCSI_FALCON, IF_CART. If zero, defaults to all IFs available on this machine.
// whichDevType -- which dev type we want to find DEV_CE, DEV_CS, DEV_OTHER. If zero, defaults to DEV_CE

BYTE findDevice(BYTE whichIF, BYTE whichDevType)
{
    __whichIF      = whichIF;
    __whichDevType = whichDevType;
    __devTypeFound = DEV_NONE;          // no device found (yet)
    __mode         = Super(SUP_INQUIRE);

    return __findDevice();
}
//--------------------------------------------------
// return found device type which was found when findDevice() did run last time
BYTE getDevTypeFound(void)
{
    return __devTypeFound;
}
