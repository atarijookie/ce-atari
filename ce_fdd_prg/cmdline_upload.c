#include <mint/osbind.h>
#include <mint/ostruct.h>

#include <stdint.h>
#include <stdio.h>

#include "acsi.h"
#include "main.h"
#include "hostmoddefs.h"
#include "defs.h"
#include "hdd_if.h"
#include "find_ce.h"
#include "vt52.h"
#include "stdlib.h"

// ------------------------------------------------------------------ 

extern BYTE deviceID;
extern BYTE commandShort[CMD_LENGTH_SHORT];
extern BYTE commandLong [CMD_LENGTH_LONG];

void showComErrorDialog(void);

extern BYTE sectorCount;
extern BYTE *pBfr, *pBfrCnt;

BYTE getCurrentSlot(void);
BYTE setCurrentSlot(BYTE newSlot);
BYTE currentSlot;

BYTE uploadImage(int index, char *customPath);
char *findFirstImageInFolder(void);

BYTE getIsImageBeingEncoded(void);
#define ENCODING_DONE       0
#define ENCODING_RUNNING    1
#define ENCODING_FAIL       2

_DTA ourDta;     // used in findFirstImageInFolder() 

void waitBeforeReset(void);

// ------------------------------------------------------------------ 
void handleCmdlineUpload(char *path, int paramsLength)
{
    BYTE found;

    // write some header out
    (void) Clear_home();
    (void) Cconws("\33f");      // cursor off
    (void) Cconws("\33p[  CosmosEx floppy TTP  ]\r\n[  by Jookie 2014-2018  ]\33q\r\n\r\n");

    path[paramsLength] = 0;                     // terminate path

    // if path is 0 or 1 byte, try to find first image in folder
    // if path is 2 or more bytes, try to load it directly as is

    if(paramsLength <= 1) {                     // single char argument given?
        path = findFirstImageInFolder();        // try to find first valid ST / MSA image in the same folder (e.g. used like this for compo presentation)
    }

    if(path == NULL) {                          // no floppy image found?
        (void) Cconws("This is a drap-and-drop, upload\r\n");
        (void) Cconws("and run floppy tool.\r\n\r\n");
        (void) Cconws("\33pArgument is path to floppy image.\33q\r\n\r\n");

        (void) Cconws("If no argument is specified, this tool\r\n");
        (void) Cconws("will try to find first image in folder.\r\n\r\n");

        (void) Cconws("If no image was found in the folder,\r\n");
        (void) Cconws("this message is shown instead.\r\n\r\n");

        (void) Cconws("For menu driven floppy config\r\n");
        (void) Cconws("run the CE_FDD.PRG\r\n");
        getKey();
        return;
    }

    paramsLength = strlen(path);                // get path length (if found by findFirstImageInFolder(), it needs to be updated)

    (void) Cconws("Filename   : ");
    (void) Cconws(path);
    (void) Cconws("\r\n");
    
    // search for CosmosEx on ACSI bus
    found = Supexec(findDevice);

    if(!found) {                                    // not found? quit
        sleep(3);
        return;
    }

    // now set up the acsi command bytes so we don't have to deal with this one anymore 
    commandShort[0] = (deviceID << 5);              // cmd[0] = ACSI_id + TEST UNIT READY (0)
    commandLong[0]  = (deviceID << 5) | 0x1f;       // cmd[0] = ACSI_id + ICD command marker (0x1f)

    BYTE res = getCurrentSlot();                    // get the current slot
    
    if(!res) {
        return;
    }

    (void) Cconws("\r\n");                          // extra line between CE find output and rest

    if(currentSlot > 2) {                           // current slot is out of index? (probably empty slot selected) Upload to slot #0
        currentSlot = 0;
    }

    // allow changing floppy uload slot before upload
    int slotChangeTime = 9;

    while(slotChangeTime >= 0) {
        int i;
        BYTE key = getKeyIfPossible();                  // get key if one is waiting or just return 0 if no key is waiting

        if(key == '1' || key == '2' || key == '3') {    // valid key? good
            currentSlot = key - '1';
            slotChangeTime = 0;
        }

        (void) Cconws("\r\33qFloppy slot: ");           // show label

        if(slotChangeTime > 0) {                        // if still waiting for key, inverse colors
            (void) Cconws("\33p");
        }

        Cconout(currentSlot + '1');                     // show current slot

        for(i=0; i<9; i++) {                            // show 'progress bar'
            if(i == slotChangeTime) {                   // turn inverse colors off after displaying remaining time (and display rest for clearing previous)
                (void) Cconws("\33q");
            }

            Cconout(' ');
        }

        slotChangeTime--;
        msleep(330);
    }

    (void) Cconws("\r\n");

    // upload the image to CosmosEx
    res = uploadImage((int) currentSlot, path);     // now try to upload

    if(!res) {
        (void) Cconws("Image upload failed, press key to terminate.\r\n");
        getKey();
        return;
    }

    // wait until image is being encoded
    (void) Cconws("Encoding   : ");

    while(1) {
        res = getIsImageBeingEncoded();

        if(res == ENCODING_DONE) {                                  // encoding went OK
            setCurrentSlot(currentSlot);
            waitBeforeReset();                                      // auto-reset after a short while, but give user a chance to cancel reset
            break;
        }

        if(res == ENCODING_FAIL) {                                  // encoding failed
            (void) Cconws("\n\r\n\rFinal phase failed, press key to terminate.\r\n");
            getKey();
            break;
        }

        msleep(500);                                                // encoding still running
        (void) Cconws("\33p \33q");                                 // show progress...
    }

    return;
}

void resetST(void)
{
    asm("move.w  #0x2700,sr\n\t"
        "move.l   0x0004.w,a0\n\t"
        "jmp     (a0)\n\t");
}

void waitBeforeReset(void)
{
    (void) Cconws("\33f");      // cursor off
    (void) Cconws("\n\r\n\rDone.\r\n\r\nST will reset itself soon.\r\nPress any key to cancel!\r\n");

    // if user doesn't press anything, the ST will reset itself...
    #define CANCEL_TIME     6
    int cancelTime = CANCEL_TIME;

    while(cancelTime >= 0) {
        int i;
        BYTE key = getKeyIfPossible();              // get key if one is waiting or just return 0 if no key is waiting

        if(key != 0) {                              // valid key? don't reset ST and quit
            (void) Cconws("\33q\n\rReset canceled by user.\r\nTerminating and returning to desktop.\r\n");
            sleep(1);
            return;
        }

        (void) Cconws("\r\33qRESET: \33p");         // show label

        for(i=0; i<CANCEL_TIME; i++) {              // show 'progress bar'
            if(i == cancelTime) {                   // turn inverse colors off after displaying remaining time (and display rest for clearing previous)
                (void) Cconws("\33q");
            }

            Cconout(' ');
        }

        cancelTime--;
        msleep(330);
    }

    // reset the ST now!
    Supexec(resetST);
}

char *findFirstImageInFolder(void)
{
    _DTA *oldDta;
    int res;

    oldDta = Fgetdta();         // get original DTA
    Fsetdta(&ourDta);           // set our temporary DTA

    res = Fsfirst("*.MSA", 0);  // find MSA image

    if(res == 0) {              // if file was found
        Fsetdta(oldDta);        // restore the original DTA
        return ourDta.dta_name;
    }

    res = Fsfirst("*.ST", 0);  // find ST image

    if(res == 0) {              // if file was found
        Fsetdta(oldDta);        // restore the original DTA
        return ourDta.dta_name;
    }

    Fsetdta(oldDta);            // restore the original DTA
    return NULL;                // nothing found
}

BYTE getCurrentSlot(void)
{
    commandShort[4] = FDD_CMD_GET_CURRENT_SLOT;

    sectorCount = 1;                            // read 1 sector

    BYTE res = Supexec(ce_acsiReadCommand); 
        
    if(res == FDD_OK) {                         // good? copy in the results
        currentSlot = pBfr[0];
        return 1;
    } 
    
    // bad? show error
    showComErrorDialog();
    return 0;
}

BYTE setCurrentSlot(BYTE newSlot)
{
    commandShort[4] = FDD_CMD_SET_CURRENT_SLOT;
    commandShort[5] = newSlot;
    
    sectorCount = 1;                            // read 1 sector

    BYTE res = Supexec(ce_acsiReadCommand); 
        
    if(res == FDD_OK) {                         // good? copy in the results
        return 1;
    } 
    
    // bad? show error
    showComErrorDialog();
    return 0;
}

BYTE getIsImageBeingEncoded(void)
{
    commandShort[4] = FDD_CMD_GET_IMAGE_ENCODING_RUNNING;
    commandShort[5] = currentSlot;

    sectorCount = 1;                            // read 1 sector

    BYTE res = Supexec(ce_acsiReadCommand); 
        
    if(res == FDD_OK) {                         // good? copy in the results
        BYTE isRunning = pBfr[0];               // isRunning - 1: is running, 0: is not running
        return isRunning;
    } 
    
    // fail?
    showComErrorDialog();                             // show error
    return ENCODING_FAIL;
}

