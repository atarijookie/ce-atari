#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <gem.h>
#include <mt_gem.h>

#include <stdint.h>
#include <stdio.h>

#include "../ce_hdd_if/stdlib.h"
#include "../ce_hdd_if/hdd_if.h"

#include "main.h"
#include "hostmoddefs.h"
#include "keys.h"
#include "defs.h"
#include "CE_FDD.H"
#include "aes.h"

// ------------------------------------------------------------------
extern BYTE deviceID;
extern BYTE commandShort[CMD_LENGTH_SHORT];

extern BYTE *p64kBlock;
extern BYTE sectorCount;

extern BYTE *pBfr, *pBfrCnt;

extern char filePath[256], fileName[256];

BYTE siloContent[512];

BYTE uploadImage(int index, char *customPath);
void swapImage(int index);
void removeImage(int index);

void newImage(int index);
void downloadImage(int index);

void createFullPath(char *fullPath, char *filePath, char *fileName);

void showMenu(char fullNotPartial);
void showImage(int index);
void getSiloContent(void);

void getStatus(void);
extern Status status;

Dialog dialogConfig;        // dialog with floppy image config

BYTE getSelectedSlotNo(void)
{
    OBJECT *o1 = (OBJECT *) &dialogConfig.tree[RADIO_SLOT1];
    OBJECT *o2 = (OBJECT *) &dialogConfig.tree[RADIO_SLOT2];
    OBJECT *o3 = (OBJECT *) &dialogConfig.tree[RADIO_SLOT3];

    if(o1->ob_state & OS_SELECTED) {
        return 1;
    }

    if(o2->ob_state & OS_SELECTED) {
        return 2;
    }

    if(o3->ob_state & OS_SELECTED) {
        return 3;
    }

    return 0;
}

void showImageFileName(int slot, const char *filename)
{
    int16_t textIdx;

    switch(slot) {  // convert slot to object index
        case 0: textIdx = IMAGENAME1; break;
        case 1: textIdx = IMAGENAME2; break;
        case 2: textIdx = IMAGENAME3; break;
        default: return;
    }

    if(!filename || filename[0] == 0) { // if filename null of empty string, set 'empty' to it
        filename = " [ empty ] ";
    }

    setObjectString(textIdx, filename);  // set net text
}

void showProgress(int percent)
{
    char progress[24];

    if(percent < 0) {   // if should hide progress
        memset(progress, ' ', 20);  // set empty string
        progress[20] = 0;           // terminate string
        setObjectString(STR_PROGRESS, progress);
        return;
    }

    if(percent > 100) { // progress value too high?
        percent = 100;
    }

    percent = percent / 10;

    strcpy(progress, "Progress: "); // start with this
    int i;
    for(i=0; i<percent; i++) {      // for each 10% draw one asterisk
        strcat(progress, "*");
    }
    progress[20] = 0;               // terminate string

    if(cd) {                        // if got current dialog specified
        setObjectString(STR_PROGRESS, progress); // update string
    } else {                        // no current dialog, output to console only
        (void) Cconws("\r");        // start of line
        (void) Cconws(progress);    // show progress string
    }
}

void showFilename(const char *filename)
{
    char fname[24];

    if(filename) {  // filename specified? show it with label
        strcpy(fname, "FileName: ");    // copy in label
        strcat(fname, filename);        // copy in filename
        fname[22] = 0;                  // terminate string
    } else {        // filename not specified? hide label
        memset(fname, ' ', 22);         // fill with spaces
        fname[22] = 0;                  // terminate string
    }

    setObjectString(STR_FILENAME, fname); // update string
}

void getAndShowSiloContent(void)
{
    getSiloContent();       // get content from device

    int i;
    for(i=0; i<3; i++) {    // show it
        showImage(i);
    }
}

BYTE gem_floppySetup(void)
{
    rsrc_gaddr(R_TREE, FDD, &dialogConfig.tree); // get address of dialog tree
    cd = &dialogConfig;             // set pointer to current dialog, so all helper functions will work with that dialog

    showDialog(TRUE);               // show dialog

    getAndShowSiloContent();        // get and show current content of slots

    showProgress(-1);      // hide progress bar
    showFilename(NULL);    // hide load/save filename

    BYTE retVal = KEY_F10;

    while(1) {
        int16_t exitobj = form_do(dialogConfig.tree, 0) & 0x7FFF;

        // unselect button
        selectButton(exitobj, FALSE);

        if(exitobj == BTN_EXIT) {
            retVal = KEY_F10;   // KEY_F10 - quit
            break;
        }

        if(exitobj == BTN_INTERNET) {   // user wants to download images from internet?
            getStatus();                // talk to CE to see the status

            if(status.doWeHaveStorage) { // if we have storage
                retVal = KEY_F9;        // KEY_F9 -- download images from internet
                break;
            } else {                    // if we don't have storage
                showErrorDialog("No USB or network storage!|Attach USB drive and try again.");
            }
        }

        BYTE slotNo = getSelectedSlotNo();

        if(!slotNo) {               // failed to get slot number? try once again
            continue;
        }

        if(exitobj == BTN_LOAD) {   // load image into slot
            uploadImage(slotNo - 1, NULL);

            showProgress(-1);      // hide progress bar
            showFilename(NULL);    // hide load/save filename
            getAndShowSiloContent();        // get and show current content of slots
            continue;
        }

        if(exitobj == BTN_SAVE) {   // save content of slot to file
            downloadImage(slotNo - 1);
            showProgress(-1);      // hide progress bar
            showFilename(NULL);    // hide load/save filename
            continue;
        }

        if(exitobj == BTN_CLEAR) {  // remove image from slot
            removeImage(slotNo - 1);
            getAndShowSiloContent();        // get and show current content of slots
            continue;
        }

        if(exitobj == BTN_NEW) {    // create new empty image in slot
            newImage(slotNo - 1);
            getAndShowSiloContent();        // get and show current content of slots
            continue;
        }
    }

    showDialog(FALSE); // hide dialog
    return retVal;
}
// ------------------------------------------------------------------
void showImage(int index)
{
    if(index < 0 || index > 2) {
        return;
    }

    // data is stored in siloContent like this:
    // offset   0: filename 1
    // offset  80: content  1
    // offset 160: filename 2
    // offset 240: content  2
    // offset 320: filename 3
    // offset 400: content  3

    BYTE *filename  = &siloContent[(index * 160)];
    showImageFileName(index, (const char *) filename);
}

void newImage(int index)
{
    if(index < 0 || index > 2) {
        return;
    }

    commandShort[4] = FDD_CMD_NEW_EMPTYIMAGE;
    commandShort[5] = index;

    sectorCount = 1;                                        // read 1 sector

    BYTE res = Supexec(ce_acsiReadCommand);

	if(res != FDD_OK) {                                     // bad? write error
        showComErrorDialog();
    }
}

void downloadImage(int index)
{
    if(index < 0 || index > 2) {        // floppy index out of range?
        return;
    }

    // start the transfer
    commandShort[4] = FDD_CMD_DOWNLOADIMG_START;
    commandShort[5] = index;

    sectorCount = 1;                                        // read 1 sector

    BYTE res = Supexec(ce_acsiReadCommand);

    if(res == FDD_OK) {                                     // good? copy in the results
        strcpy(fileName, (char *) pBfr);                    // pBfr should contain original file name
    } else {                                                // bad? show error
        showComErrorDialog();
        return;
    }

    //--------------------------------
    // open fileselector and get path
    showDialog(FALSE);                    // hide dialog

    short button;
    fsel_input(filePath, fileName, &button);            // show file selector

    showDialog(TRUE);                     // show dialog

    if(button != 1) {                                   // if OK was not pressed
        commandShort[4] = FDD_CMD_DOWNLOADIMG_DONE;
        commandShort[5] = index;

        sectorCount = 1;                                // read 1 sector

        res = Supexec(ce_acsiReadCommand);

        if(res != FDD_OK) {
            showComErrorDialog();
        }
        return;
    }

    showFilename(fileName);            // show filename in dialog

    // fileName contains the filename
    // filePath contains the path with search wildcards, e.g.: C:\\*.*

    // create full path
    char fullPath[512];
    createFullPath(fullPath, filePath, fileName);       // fullPath = filePath + fileName

    //--------------------
    // check if can do on device copy, and do it if possible
    commandShort[4] = FDD_CMD_DOWNLOADIMG_ONDEVICE;
    commandShort[5] = index;

    sectorCount = 1;                                        // write 1 sector

    p64kBlock   = pBfr;                                     // use this buffer for writing
    strcpy((char *) pBfr, fullPath);                        // and copy in the full path

    res = Supexec(ce_acsiWriteBlockCommand);                // send atari path to host, so host can check if it's ON DEVICE COPY

    if(res == FDD_RES_ONDEVICECOPY) {                       // if host replied with this, the file is copied, nothing to do
        return;
    }
    //--------------------

    short fh = Fcreate(fullPath, 0);               		    // open file for writing

    if(fh < 0) {
        showErrorDialog("Failed to create the file.");
        return;
    }

    // do the transfer
    int32_t blockNo, len, ires;
    BYTE failed = 0;
    int progress = 0;

    for(blockNo=0; blockNo<64; blockNo++) {                 // try to get all blocks
        showProgress(progress);                    // update progress bar
        progress += 9;                                      // 720 kB image is made of 11.25 blocks of 64k, each is 8.8% of whole

        commandShort[4] = FDD_CMD_DOWNLOADIMG_GETBLOCK;     // receiving block
        commandShort[5] = (index << 6) | (blockNo & 0x3f);  // for this index and block #

        sectorCount = 128;                                  // read 128 sectors (64 kB)

        res = Supexec(ce_acsiReadCommand);                  // get the data

        if(res != FDD_OK) {                                 // error? write error
            showComErrorDialog();

            failed = 1;
            break;
        }

        len = (((WORD) pBfr[0]) << 8) | ((WORD) pBfr[1]);   // retrieve count of data in buffer

        if(len > 0) {                                       // something to write?
            ires = Fwrite(fh, len, pBfr + 2);

            if(ires < 0 || ires != len) {                   // failed to write?
                showErrorDialog("Writing to file failed!");
                failed = 1;
                break;
            }
        }

        if(len < (65536 - 2)) {                             // if received less than full 64kB block, then this was the last block
            break;
        }
    }

    // finish the transfer and close the file
    Fclose(fh);

    commandShort[4] = FDD_CMD_DOWNLOADIMG_DONE;
    commandShort[5] = index;

    sectorCount = 1;                            // read 1 sector

    res = Supexec(ce_acsiReadCommand);

    if(res != FDD_OK) {
        showComErrorDialog();
    }

    // in case of error delete the probably incomplete file
    if(failed) {
        Fdelete(fullPath);
    }
}

void getSiloContent(void)
{
    commandShort[4] = FDD_CMD_GETSILOCONTENT;

    sectorCount = 1;                            // read 1 sector

    BYTE res = Supexec(ce_acsiReadCommand);

	if(res == FDD_OK) {                         // good? copy in the results
        memcpy(siloContent, pBfr, 512);
    } else {                                    // bad? show error
        showComErrorDialog();
    }
}

void swapImage(int index)
{
    if(index < 0 || index > 2) {
        return;
    }

    commandShort[4] = FDD_CMD_SWAPSLOTS;
    commandShort[5] = index;

    sectorCount = 1;                            // read 1 sector

    BYTE res = Supexec(ce_acsiReadCommand);

    if(res != FDD_OK) {                         // bad? write error
        showComErrorDialog();
    }
}

void removeImage(int index)
{
    if(index < 0 || index > 2) {
        return;
    }

    commandShort[4] = FDD_CMD_REMOVESLOT;
    commandShort[5] = index;

    sectorCount = 1;                            // read 1 sector

    BYTE res = Supexec(ce_acsiReadCommand);

    if(res != FDD_OK) {                         // bad? write error
        showComErrorDialog();
    }
}

BYTE uploadImage(int index, char *customPath)
{
    if(index < 0 || index > 2) {
        return FALSE;
    }

    char fullPath[512];
    short fh = -1;

    if(!customPath) {                       // if customPath not specified, ask for it from user
        // open fileselector and get path
        showDialog(FALSE);                  // hide dialog

        short button;
        fsel_input(filePath, fileName, &button);    // show file selector

        showDialog(TRUE);                   // show dialog

        if(button != 1) {                   // if OK was not pressed
            return FALSE;
        }

        showFilename(fileName);             // show filename in dialog

        // fileName contains the filename
        // filePath contains the path with search wildcards, e.g.: C:\\*.*

        // create full path
        createFullPath(fullPath, filePath, fileName);       // fullPath = filePath + fileName
        fh = Fopen(fullPath, 0);            // open file for reading
    } else {                                // if got custom path
        fh = Fopen(customPath, 0);          // open file for reading
    }

    if(fh < 0) {
        showErrorDialog("Failed to open the file.");
        return FALSE;
    }

    //---------------
    // tell the device the path and filename of the source image

    commandShort[4] = FDD_CMD_UPLOADIMGBLOCK_START;             // starting image upload
    commandShort[5] = index;                                    // for this index

    p64kBlock       = pBfr;                                     // use this buffer for writing

    if(customPath) {        // got custom path?
        strcpy((char *) pBfr, customPath);                      // copy in the custom path
    } else {                // don't have custom path?
        strcpy((char *) pBfr, fullPath);                        // copy in the user path
    }

    sectorCount     = 1;                                        // write just one sector

    BYTE res;
    res = Supexec(ce_acsiWriteBlockCommand);

    if(res == FDD_RES_ONDEVICECOPY) {                           // if the device returned this code, it means that it could do the image upload / copy on device, no need to upload it from ST!
        Fclose(fh);
        return TRUE;
    }

    if(res != FDD_OK) {                                         // bad? write error
        showComErrorDialog();

        Fclose(fh);                                             // close file and quit
        return FALSE;
    }
    //---------------
    // upload the image by 64kB blocks
    BYTE good = 1;
    BYTE blockNo = 0;

    sectorCount = 128;                                          // write 128 sectors (64 kB)
    int progress = 0;

    while(1) {
        showProgress(progress);                        // update progress bar
        progress += 9;                                          // 720 kB image is made of 11.25 blocks of 64k, each is 8.8% of whole

        long len = Fread(fh, SIZE64K, pBfr);

        if(len < 0) {                                           // error while reading the file?
            good = 0;                                           // mark that upload didn't finish good
            break;
        }

        if(len == SIZE64K) {                                    // full block was read?
            p64kBlock = pBfr;                                   // write data to ACSI from this address

            commandShort[4] = FDD_CMD_UPLOADIMGBLOCK_FULL;      // sending full block
            commandShort[5] = (index << 6) | (blockNo & 0x3f);  // for this index and block #
        } else {                                                // partial block was read?
            p64kBlock = pBfrCnt;                                // write data to ACSI from this address

            commandShort[4] = FDD_CMD_UPLOADIMGBLOCK_PART;      // sending full block
            commandShort[5] = (index << 6) | (blockNo & 0x3f);  // for this index and block #

            pBfrCnt[0] = len >> 8;
            pBfrCnt[1] = len & 0xff;
        }

        res = Supexec(ce_acsiWriteBlockCommand);                // send the data

        if(res != FDD_OK) {                                     // error? write error
            showComErrorDialog();

            good = 0;                                           // mark that upload didn't finish good
            break;
        }

        // write of block was OK, and this was the partial (last) block?
        if(commandShort[4] == FDD_CMD_UPLOADIMGBLOCK_PART) {
            good = 1;
            break;
        }

        blockNo++;
    }

    Fclose(fh);                                 // close the file

    // now tell the device if it went good or bad
    if(good == 1) {
        commandShort[4] = FDD_CMD_UPLOADIMGBLOCK_DONE_OK;
    } else {
        commandShort[4] = FDD_CMD_UPLOADIMGBLOCK_DONE_FAIL;
    }

    commandShort[5] = index;

    sectorCount = 1;                            // read 1 sector

    res = Supexec(ce_acsiReadCommand);

    if(res != FDD_OK) {                         // bad? write error
        showErrorDialog("Failed to finish upload.");
    }

    return TRUE;
}
