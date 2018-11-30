//--------------------------------------------------
#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <support.h>

#include <stdint.h>
#include <stdio.h>
#include <gem.h>
#include <mt_gem.h>

#include "stdlib.h"
#include "acsi.h"
#include "find_ce.h"
#include "hdd_if.h"
#include "translated.h"
#include "hostmoddefs.h"

extern BYTE  deviceID;
extern BYTE  cosmosExNotCosmoSolo;
extern BYTE *pDmaBuffer;

BYTE findCE(BYTE hddIf);
BYTE ce_identify(BYTE id, BYTE hddIf);

//--------------------------------------------------

BYTE getMachineType(void);
BYTE machine;

//--------------------------------------------------
// following code is used to define a simple dialog for showing progress of scan using GEM
unsigned char SCAN_RSC[] = {
  0x00, 0x00, 0x00, 0x28, 0x00, 0x88, 0x00, 0x88, 0x00, 0x88, 0x00, 0x88,
  0x00, 0x88, 0x00, 0xb8, 0x00, 0xb8, 0x00, 0x24, 0x00, 0x04, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xb8,
  0x00, 0x00, 0x00, 0x28, 0xff, 0xff, 0x00, 0x01, 0x00, 0x03, 0x00, 0x14,
  0x00, 0x00, 0x00, 0x10, 0x00, 0x02, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x16, 0x00, 0x06, 0x00, 0x02, 0xff, 0xff, 0xff, 0xff, 0x00, 0x1c,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0x00, 0x01, 0x00, 0x01,
  0x00, 0x14, 0x00, 0x01, 0x00, 0x03, 0xff, 0xff, 0xff, 0xff, 0x00, 0x1c,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9d, 0x00, 0x01, 0x00, 0x02,
  0x00, 0x14, 0x00, 0x01, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x00, 0x1a,
  0x00, 0x27, 0x00, 0x00, 0x00, 0x00, 0x00, 0xb2, 0x00, 0x07, 0x00, 0x04,
  0x00, 0x08, 0x00, 0x01, 0x4c, 0x6f, 0x6f, 0x6b, 0x69, 0x6e, 0x67, 0x20,
  0x66, 0x6f, 0x72, 0x20, 0x43, 0x6f, 0x73, 0x6d, 0x6f, 0x73, 0x45, 0x78,
  0x00, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0x51, 0x75,
  0x69, 0x74, 0x00, 0x00
};
unsigned int SCAN_RSC_len = 184;

#define SCAN             0   /* Form/Dialog-box */
#define ROOTBOX          0   /* BOX in tree SCAN */
#define TITLE            1   /* STRING in tree SCAN */
#define STATUS           2   /* STRING in tree SCAN */
#define BTNQUIT          3   /* BUTTON in tree SCAN */

int maxStatusLen = 0;       // maximu length of status
OBJECT *scanDialogTree;     // this gets filled by getScanDialogTree() and then used when scanning

OBJECT *getScanDialogTree(void)
{
    RSHDR *rsc = (RSHDR *) SCAN_RSC;
    OBJECT *tree = (OBJECT *) (SCAN_RSC + rsc->rsh_object);     // pointer to first obj

    int i;
    for(i=0; i<rsc->rsh_nobs; i++) {                    // go through all the objects
        if(tree[i].ob_type == G_STRING || tree[i].ob_type == G_BUTTON) {  // if it's string or button, fix pointer to its text
            char *p = (char *) SCAN_RSC;                // pointer to start of RSC file
            p += (int) tree[i].ob_spec.free_string;     // add value from ob_spec, which is currently offset
            tree[i].ob_spec.free_string = (char *) p;   // store pointer back to ob_spec, which points now to the string
        }

        rsrc_obfix(tree, i);                            // convert coordinates from character-based to pixel-based
    }

    maxStatusLen = strlen(tree[STATUS].ob_spec.free_string);    // find out maximum status length
    scanDialogTree = tree;  // store it here, will use it then
    return tree;            // return pointer to dialog tree
}

void showStatusInGem(char *status)
{
    if(!scanDialogTree) {           // no dialog? quit
        return;
    }

    int newLen = strlen(status);

    if(newLen <= maxStatusLen) {                                        // new string shorter or exact as it should be?
        strcpy(scanDialogTree[STATUS].ob_spec.free_string, status);               // copy in whole string
    } else {                                                            // new string longer than it should be? clip it
        memcpy(scanDialogTree[STATUS].ob_spec.free_string, status, maxStatusLen); // copy part of the string
        scanDialogTree[STATUS].ob_spec.free_string[maxStatusLen] = 0;             // zero terminate string
    }

    int16_t ox, oy;
    OBJECT *obj = &scanDialogTree[STATUS];                  // pointer to status string object
    objc_offset(scanDialogTree, STATUS, &ox, &oy);          // get current screen coordinates of object
    objc_draw(scanDialogTree, ROOT, MAX_DEPTH, ox - 2, oy - 2, obj->ob_width + 4, obj->ob_height + 4); // draw object tree, but clip only to text position and size + some pixels more around to hide button completely
}

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
        if(scanDialogTree) {    // got dialog? show in dialog
            showStatusInGem("Device not found.");
            key = 'Q';
            sleep(1);
        } else {                // no dialog? show in console
            (void) Cconws("\n\rDevice not found.\n\rPress any key to retry or 'Q' to quit.\n\r");
            key = Cnecin();
        }

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

    if(!scanDialogTree) {           // no dialog?
        (void) Cconws("\n\rLooking for CosmosEx on ");
        (void) Cconws((hddIf == IF_ACSI) ? "ACSI: " : "SCSI: ");
    }

    hdd_if_select(hddIf);                               // select HDD IF

    for(id=0; id<8; id++) {                             // try to talk to all ACSI devices
        if(scanDialogTree) {    // got dialog? show in dialog
            char tmp[20];
            strcpy(tmp, "xCSI: X ");
            tmp[0] = (hddIf == IF_ACSI) ? 'A' : 'S';    // ACSI / SCSI 
            tmp[6] = '0' + id;  // ID

            showStatusInGem(tmp);
        } else {                // no dialog? show in console
            Cconout('0' + id);                          // write out BUS ID
        }

        res = ce_identify(id, hddIf);                   // try to read the IDENTITY string 

        if(res == 1) {                                  // if found the CosmosEx 
            if(scanDialogTree) {    // got dialog? show in dialog
                char tmp[20];
                strcpy(tmp, "Found on xCSI X ");
                tmp[9] = (hddIf == IF_ACSI) ? 'A' : 'S';    // ACSI / SCSI 
                tmp[14] = '0' + id;  // ID
                showStatusInGem(tmp);
            } else {                // no dialog? show in console
                (void) Cconws(" <-- found!\n\r");
            }

            deviceID = id;               // store the BUS ID of device
            return TRUE;
        }
    }
  
    return FALSE;
}
//--------------------------------------------------
BYTE ce_identify(BYTE id, BYTE hddIf)
{
    BYTE cmd[] = {0, 'C', 'E', HOSTMOD_FDD_SETUP, FDD_CMD_IDENTIFY, 0};
  
    cmd[0] = (id << 5);                             // cmd[0] = ACSI_id + TEST UNIT READY (0)
    memset(pDmaBuffer, 0, 512);                     // clear the buffer 
  
    (*hdIf.cmd)(1, cmd, 6, pDmaBuffer, 1);          // issue the identify command and check the result 
    
    if(!hdIf.success || hdIf.statusByte != OK) {    // if failed, return FALSE
        return FALSE;
    }

    if(strncmp((char *) pDmaBuffer, "CosmosEx floppy setup", 21) != 0) {     // the identity string doesn't match?
        return FALSE;
    }
    
    return TRUE;                                    // success 
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

