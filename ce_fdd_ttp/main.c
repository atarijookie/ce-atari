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

BYTE deviceID;
BYTE commandShort[CMD_LENGTH_SHORT] = { 0, 'C', 'E', HOSTMOD_FDD_SETUP, 0, 0};

void showComError(void);

BYTE *p64kBlock;
BYTE sectorCount;

BYTE *pBfrOrig;
BYTE *pBfr, *pBfrCnt;
BYTE *pDmaBuffer;

BYTE atariKeysToSingleByte(BYTE vkey, BYTE key);

BYTE getCurrentSlot(void);
BYTE setCurrentSlot(BYTE newSlot);
BYTE currentSlot;

BYTE uploadImage(int index, char *path);
char *findFirstImageInFolder(void);

BYTE getIsImageBeingEncoded(void);
#define ENCODING_DONE       0
#define ENCODING_RUNNING    1
#define ENCODING_FAIL       2

_DTA ourDta;     // used in findFirstImageInFolder() 

void waitBeforeReset(void);

// ------------------------------------------------------------------ 
int main( int argc, char* argv[] )
{
    BYTE found;

    // write some header out
    (void) Clear_home();
    (void) Cconws("\33f");      // cursor off
    (void) Cconws("\33p[  CosmosEx floppy TTP  ]\r\n[  by Jookie 2014-2018  ]\33q\r\n\r\n");

    char *params        = (char *) argv;            // get pointer to params (path to file)
    int paramsLength    = (int) params[0];
    char *path          = params + 1;

    if(paramsLength == 0) {                         // no TTP argument given?
        path = findFirstImageInFolder();            // try to find first valid ST / MSA image in the same folder (e.g. used like this for compo presentation)

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
            return 0;
        }

        paramsLength = strlen(path);                // get path length

        (void) Cconws("Found image: ");
        (void) Cconws(path);
        (void) Cconws("\r\n");
    }

    path[paramsLength]  = 0;                        // terminate path
    
    pBfrOrig = (BYTE *) Malloc(SIZE64K + 4);

    if(pBfrOrig == NULL) {
        (void) Cconws("\r\nMalloc failed!\r\n");
        sleep(3);
        return 0;
    }

    DWORD val = (DWORD) pBfrOrig;
    pBfr      = (BYTE *) ((val + 4) & 0xfffffffe);  // create even pointer
    pBfrCnt   = pBfr - 2;                           // this is previous pointer - size of WORD 

    pDmaBuffer = pBfr;

    // search for CosmosEx on ACSI bus
    found = Supexec(findDevice);

    if(!found) {                                    // not found? quit
        sleep(3);
        return 0;
    }

    // now set up the acsi command bytes so we don't have to deal with this one anymore 
    commandShort[0] = (deviceID << 5);              // cmd[0] = ACSI_id + TEST UNIT READY (0)   

    BYTE res = getCurrentSlot();                    // get the current slot
    
    if(!res) {
        Mfree(pBfrOrig);
        return 0;
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
        Mfree(pBfrOrig);
        return 0;
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

    Mfree(pBfrOrig);
    return 0;
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

void intToStr(int val, char *str)
{
    int i3, i2, i1;
    i3 = (val / 100);               // 123 / 100 = 1
    i2 = (val % 100) / 10;          // (123 % 100) = 23, 23 / 10 = 2
    i1 = (val % 10);                // 123 % 10 = 3

    str[0] = i3 + '0';
    str[1] = i2 + '0';
    str[2] = i1 + '0';

    if(val < 100) {
        str[0] = ' ';
    }
    
    if(val < 10) {
        str[1] = ' ';
    }
    
    str[3] = 0;                     // terminating zero
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

BYTE getKey(void)
{
    DWORD scancode;
    BYTE key, vkey;

    scancode = Cnecin();                    /* get char form keyboard, no echo on screen */

    vkey    = (scancode >> 16)  & 0xff;
    key     =  scancode         & 0xff;

    key     = atariKeysToSingleByte(vkey, key); /* transform BYTE pair into single BYTE */
    
    return key;
}

void removeLastPartUntilBackslash(char *str)
{
    int i, len;
    
    len = strlen(str);
    
    for(i=(len-1); i>= 0; i--) {
        if(str[i] == '\\') {
            break;
        }
    
        str[i] = 0;
    }
}

// make single ACSI read command by the params set in the commandShort buffer
BYTE ce_acsiReadCommand(void)
{
    commandShort[0] = (deviceID << 5);                                          // cmd[0] = ACSI_id + TEST UNIT READY (0)   
  
    memset(pBfr, 0, 512);                                                       // clear the buffer 

    (*hdIf.cmd)(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pBfr, sectorCount);   // issue the command and check the result 

    if(!hdIf.success) {
        return 0xff;
    }
    
    return hdIf.statusByte;
}

BYTE ce_acsiWriteBlockCommand(void)
{
    commandShort[0] = (deviceID << 5);                                          // cmd[0] = ACSI_id + TEST UNIT READY (0)   
  
    (*hdIf.cmd)(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, p64kBlock, sectorCount);    // issue the command and check the result 

    if(!hdIf.success) {
        return 0xff;
    }
    
    return hdIf.statusByte;
}

BYTE getLowestDrive(void)
{
    BYTE i;
    DWORD drvs = Drvmap();
    DWORD mask;
    
    for(i=2; i<16; i++) {                                                       // go through the available drives
        mask = (1 << i);
        
        if((drvs & mask) != 0) {                                                // drive is available?
            return ('A' + i);
        }
    }
    
    return 'A';
}

void showComError(void)
{
    (void) Clear_home();
    (void) Cconws("Error in CosmosEx communication!\r\n");
    Cnecin();
}

void createFullPath(char *fullPath, char *filePath, char *fileName)
{
    strcpy(fullPath, filePath);
    
    removeLastPartUntilBackslash(fullPath);             // remove the search wildcards from the end

    if(strlen(fullPath) > 0) {                          
        if(fullPath[ strlen(fullPath) - 1] != '\\') {   // if the string doesn't end with backslash, add it
            strcat(fullPath, "\\");
        }
    }
    
    strcat(fullPath, fileName);                         // add the filename
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
    showComError();
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
    showComError();
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
    showComError();                             // show error
    return ENCODING_FAIL;
}

void logMsg(char *logMsg)
{
//    if(showLogs) {
//        (void) Cconws(logMsg);
//    }
}

void logMsgProgress(DWORD current, DWORD total)
{
//    if(!showLogs) {
//        return;
//    }

//    (void) Cconws("Progress: ");
//    showHexDword(current);
//    (void) Cconws(" out of ");
//    showHexDword(total);
//    (void) Cconws("\n\r");
}

