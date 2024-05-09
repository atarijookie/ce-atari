#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <unistd.h>
#include <gem.h>
#include <mt_gem.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../libacsiscsi/acsi.h"
#include "../libacsiscsi/find_ce.h"
#include "../libacsiscsi/hdd_if.h"
#include "main.h"
#include "hostmoddefs.h"
#include "keys.h"
#include "defs.h"
#include "CE_FDD.H"
#include "aes.h"

// ------------------------------------------------------------------
uint8_t deviceID;
uint8_t commandShort[CMD_LENGTH_SHORT] = {         0, 'C', 'E', HOSTMOD_FDD_SETUP, 0, 0};
uint8_t commandLong [CMD_LENGTH_LONG]  = {0x1f, 0xA0, 'C', 'E', HOSTMOD_FDD_SETUP, 0, 0, 0, 0, 0, 0, 0, 0};

void createFullPath(char *fullPath, char *filePath, char *fileName);

char filePath[256], fileName[256];

uint8_t *p64kBlock;
uint8_t sectorCount;

uint8_t *pBfrOrig;
uint8_t *pBfr, *pBfrCnt;
uint8_t *pDmaBuffer;

uint8_t loopForDownload(void);

uint8_t gem_floppySetup(void);
uint8_t gem_imageDownload(void);

void handleCmdlineUpload(char *path, int paramsLength);

// uncomment following for development without device
//#define NODEVICE

OBJECT *getScanDialogTree(void);

// ------------------------------------------------------------------
int main(int argc, char** argv)
{
    Goto_pos(0,0);
    pBfrOrig = (uint8_t *) Malloc(SIZE64K + 4);

    if(pBfrOrig == NULL) {
        (void) Cconws("Malloc failed!\r\n");
        sleep(3);
        return 0;
    }

    uint32_t val = (uint32_t) pBfrOrig;
    pBfr      = (uint8_t *) ((val + 4) & 0xfffffffe);  // create even pointer
    pBfrCnt   = pBfr - 2;           // this is previous pointer - size of uint16_t

    pDmaBuffer = pBfr;

    // init fileselector path
    strcpy(filePath, "C:\\*.*");
    memset(fileName, 0, 256);

    uint8_t drive = getLowestDrive();  // get the lowest HDD letter and use it in the file selector
    filePath[0] = drive;

#ifdef OUTPUT_TTP
    // if compiled with OUTPUT_TTP, this app will work without GEM UI - as the TTP
    char *params = (char *) argv;           // get pointer to params (path to file)
    int   paramsLength = (int) params[0];   // 0th byte -- strlen(arguments)
    char *path = params + 1;                // path to file

    handleCmdlineUpload(path, paramsLength);
    Mfree(pBfrOrig);
    return 0;
#endif

    // if compiled without OUTPUT_TTP, this app will work with GEM UI - as the PRG
    uint8_t res = gem_init();          // initialize GEM stuff

    if(!res) {                      // gem init failed? quit then
        Mfree(pBfrOrig);
        return 0;
    }

#ifndef NODEVICE
/*
    Scanning with GEM currently disabled, as this findDevice() is used all in supervisor mode
    and that is probably causing the crash on return. The findDevice() should be altered to 
    run in user mode and switch to supervisor only for hw access...

    Dialog scanDialog;
    scanDialog.tree = getScanDialogTree();   // get pointer to GEM dialog definition
    cd = &scanDialog;

    showDialog(TRUE);                           // show GEM dialog
*/

    // search for CosmosEx on ACSI & SCSI bus
	// search for CosmosEx on ACSI & SCSI bus
    res = findDevice(FIND_DEV_CE | FIND_DEV_CS);

    if(res == DEVICE_NOT_FOUND) {
        gem_deinit();               // deinit GEM
        Mfree(pBfrOrig);
        return 0;
    }

    deviceID = res & 0x07;                                  // store the BUS ID of device

//  showDialog(FALSE);                          // hide GEM dialog

#endif

    // now set up the acsi command bytes so we don't have to deal with this one anymore
    commandShort[0] = (deviceID << 5);          // cmd[0] = ACSI_id + TEST UNIT READY (0)
    commandLong[0]  = (deviceID << 5) | 0x1f;   // cmd[0] = ACSI_id + ICD command marker (0x1f)

    while(1) {
        res = gem_floppySetup();    // show and handle floppy setup via dialog

        if(res == KEY_F10) {        // should quit?
            break;
        }

        res = gem_imageDownload();    // show and handle floppy image download

        if(res == KEY_F10) {        // should quit?
            break;
        }
    }

    gem_deinit();                   // deinit GEM
    Mfree(pBfrOrig);
    return 0;
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

// make single ACSI read command by the params set in the commandLong buffer
uint8_t ce_acsiReadCommandLong(void)
{
    memset(pBfr, 0, 512);               // clear the buffer

#ifdef NODEVICE
    return 0;
#endif

    (*hdIf.cmd)(ACSI_READ, commandLong, CMD_LENGTH_LONG, pBfr, sectorCount);   // issue the command and check the result

    if(!hdIf.success) {
        return 0xff;
    }

    return hdIf.statusByte;
}

// make single ACSI read command by the params set in the commandShort buffer
uint8_t ce_acsiReadCommand(void)
{
    memset(pBfr, 0, 512);                                                       // clear the buffer

#ifdef NODEVICE
    return 0;
#endif

    (*hdIf.cmd)(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pBfr, sectorCount);   // issue the command and check the result 

    if(!hdIf.success) {
        return 0xff;
    }

    return hdIf.statusByte;
}

uint8_t ce_acsiWriteBlockCommand(void)
{
#ifdef NODEVICE
    return 0;
#endif

    (*hdIf.cmd)(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, p64kBlock, sectorCount);    // issue the command and check the result 

    if(!hdIf.success) {
        return 0xff;
    }

    return hdIf.statusByte;
}

uint8_t getLowestDrive(void)
{
    uint8_t i;
    uint32_t drvs = Drvmap();
    uint32_t mask;

    for(i=2; i<16; i++) {                                                       // go through the available drives
        mask = (1 << i);

        if((drvs & mask) != 0) {                                                // drive is available?
            return ('A' + i);
        }
    }

    return 'A';
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
