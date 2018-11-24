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

#include "acsi.h"
#include "main.h"
#include "hostmoddefs.h"
#include "keys.h"
#include "defs.h"
#include "FDD.H"

// ------------------------------------------------------------------
extern BYTE deviceID;
extern BYTE commandShort[CMD_LENGTH_SHORT];

extern BYTE *p64kBlock;
extern BYTE sectorCount;

extern BYTE *pBfr, *pBfrCnt;

extern char filePath[256], fileName[256];

BYTE siloContent[512];

void uploadImage(int index);
void swapImage(int index);
void removeImage(int index);

void newImage(int index);
void downloadImage(int index);

void createFullPath(char *fullPath, char *filePath, char *fileName);

BYTE loopForSetup(void);
void showMenu(char fullNotPartial);
void showImage(int index);
void getSiloContent(void);

extern TDestDir destDir;

BYTE getSelectedSlotNo(void)
{
    int32_t s1, s2, s3;
    rsrc_gaddr(R_OBJECT, RADIO_SLOT1, &s1);
    rsrc_gaddr(R_OBJECT, RADIO_SLOT2, &s2);
    rsrc_gaddr(R_OBJECT, RADIO_SLOT3, &s3);

    if(!s1 || !s2 || !s3) {
        return 0;
    }

    OBJECT *o1 = (OBJECT *) s1;
    OBJECT *o2 = (OBJECT *) s2;
    OBJECT *o3 = (OBJECT *) s3;

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

typedef struct {
    OBJECT *tree;   // pointer to the tree
    int16_t xdial, ydial, wdial, hdial; // dimensions and position of dialog
} Dialog;

Dialog dialog;      // for easier passing to helper functions store everything needed in the struct

void unselectButton(Dialog *d, int btnIdx)
{
    OBJECT *btn;
    rsrc_gaddr(R_OBJECT, btnIdx, &btn);    // get address of button

    if(!btn) {      // object not found? quit
        return;
    }

    btn->ob_state = btn->ob_state & (~OS_SELECTED);     // remove SELECTED flag

    objc_draw(d->tree, btnIdx, 0, d->xdial, d->ydial, d->wdial, d->hdial);    // draw object tree - starting with the button
}

void setObjectString(Dialog *d, int16_t objId, const char *newString)
{
    OBJECT *obj;
    rsrc_gaddr(R_OBJECT, objId, &obj);  // get address of object

    if(!obj) {                          // object not found? quit
        return;
    }

    int16_t ox, oy;
    objc_offset(d->tree, objId, &ox, &oy);          // get current screen coordinates of object

    strcpy(obj->ob_spec.free_string, newString);    // copy in the string
    objc_draw(d->tree, ROOT, MAX_DEPTH, ox, oy, obj->ob_width, obj->ob_height); // draw object tree, but clip only to text position and size
}

void showImageFileName(Dialog *d, int slot, const char *filename)
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

    setObjectString(d, textIdx, filename);  // set net text
}

void showProgress(Dialog *d, int percent)
{
    char progress[24];

    if(percent < 0) {   // if should hide progress
        memset(progress, ' ', 20);  // set empty string
        progress[20] = 0;           // terminate string
        setObjectString(d, STR_PROGRESS, progress);
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

    setObjectString(d, STR_PROGRESS, progress); // update string
}

void showFilename(Dialog *d, const char *filename)
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

    setObjectString(d, STR_FILENAME, fname); // update string
}

void getAndShowSiloContent(void)
{
    getSiloContent();       // get content from device

    int i;
    for(i=0; i<3; i++) {    // show it
        showImage(i);
    }
}

void showSetupDialog(Dialog *d, BYTE show)
{
    if(show) {  // on show
        form_center(d->tree, &d->xdial, &d->ydial, &d->wdial, &d->hdial);       // center object
        form_dial(0, 0, 0, 0, 0, d->xdial, d->ydial, d->wdial, d->hdial);       // reserve screen space for dialog
        objc_draw(d->tree, ROOT, MAX_DEPTH, d->xdial, d->ydial, d->wdial, d->hdial);  // draw object tree
    } else {    // on hide
        form_dial (3, 0, 0, 0, 0, d->xdial, d->ydial, d->wdial, d->hdial);      // release screen space
    }
}

void showErrorDialog(char *errorText)
{
    showSetupDialog(&dialog, FALSE);    // hide dialog

    char tmp[256];
    strcpy(tmp, "[3][");                // STOP icon
    strcat(tmp, errorText);             // error text
    strcat(tmp, "][ OK ]");             // OK button

    form_alert(1, tmp);                 // show alert dialog, 1st button as default one, with text and buttons in tmp[]

    showSetupDialog(&dialog, TRUE);     // show dialog
}

void showComErrorDialog(void)
{
    showErrorDialog("Error in CosmosEx communication!");
}

BYTE gem_floppySetup(void)
{
    rsrc_gaddr(R_TREE, FDD, &dialog.tree); // get address of dialog tree

    showSetupDialog(&dialog, TRUE); // show dialog

    getAndShowSiloContent();        // get and show current content of slots

    showProgress(&dialog, -1);      // hide progress bar
    showFilename(&dialog, NULL);    // hide load/save filename

    BYTE retVal = KEY_F10;

    while(1) {
        int16_t exitobj = form_do(dialog.tree, 0) & 0x7FFF;

        if(exitobj == BTN_EXIT) {
            retVal = KEY_F10;   // KEY_F10 - quit
            break;
        }

        if(exitobj == BTN_INTERNET) {
            retVal = KEY_F9;   // KEY_F9 -- download images from internet
            break;
        }

        // unselect button
        unselectButton(&dialog, exitobj);

        BYTE slotNo = getSelectedSlotNo();

        if(!slotNo) {               // failed to get slot number? try once again
            continue;
        }

        if(exitobj == BTN_LOAD) {   // load image into slot
//            uploadImage(slotNo - 1);

            showProgress(&dialog, -1);      // hide progress bar
            showFilename(&dialog, NULL);    // hide load/save filename
            getAndShowSiloContent();        // get and show current content of slots
            continue;
        }

        if(exitobj == BTN_CLEAR) {  // remove image from slot
//            removeImage(slotNo - 1);
            getAndShowSiloContent();        // get and show current content of slots
            continue;
        }

        if(exitobj == BTN_NEW) {    // create new empty image in slot
//            newImage(slotNo - 1);
            getAndShowSiloContent();        // get and show current content of slots
            continue;
        }

        if(exitobj == BTN_SAVE) {   // save content of slot to file
 //           downloadImage(slotNo - 1);
            showProgress(&dialog, -1);      // hide progress bar
            showFilename(&dialog, NULL);    // hide load/save filename
            continue;
        }
    }

    showSetupDialog(&dialog, FALSE); // hide dialog
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
    showImageFileName(&dialog, index, (const char *) filename);
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
    if((index < 0 || index > 2) && index != 10) {           // if it's not normal floppy index and not internet download index
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
    char fullPath[512];

    if(index == 10) {                                       // for internet download just use predefined destination dir
        strcpy(fullPath, destDir.path);                     // create path, e.g. 'C:\destdir\images\A_001.ST'

        int len = strlen(fullPath);
        if(fullPath[len - 1] != '\\') {                     // if the path doesn't end with \\, add it there
            strcat(fullPath, "\\");
        }

        strcat(fullPath, fileName);
    } else {                                                // for ordinary floppy slots open file selector
       // open fileselector and get path
        showSetupDialog(&dialog, FALSE);                    // hide dialog

        short button;
        fsel_input(filePath, fileName, &button);            // show file selector

        showSetupDialog(&dialog, TRUE);                     // show dialog

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

        showFilename(&dialog, fileName);            // show filename in dialog

      	// fileName contains the filename
        // filePath contains the path with search wildcards, e.g.: C:\\*.*

        // create full path
        createFullPath(fullPath, filePath, fileName);       // fullPath = filePath + fileName

        (void) Clear_home();
    }

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
        showProgress(&dialog, progress);                    // update progress bar

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

void uploadImage(int index)
{
    if(index < 0 || index > 2) {
        return;
    }

    // open fileselector and get path
    showSetupDialog(&dialog, FALSE);            // hide dialog

    short button;
    fsel_input(filePath, fileName, &button);    // show file selector

    showSetupDialog(&dialog, TRUE);             // show dialog

    if(button != 1) {                           // if OK was not pressed
        return;
    }

    showFilename(&dialog, fileName);            // show filename in dialog

	// fileName contains the filename
	// filePath contains the path with search wildcards, e.g.: C:\\*.*

    // create full path
    char fullPath[512];
    createFullPath(fullPath, filePath, fileName);       // fullPath = filePath + fileName

    short fh = Fopen(fullPath, 0);               		// open file for reading

    if(fh < 0) {
        showErrorDialog("Failed to open the file.");
        return;
    }

    //---------------
    // tell the device the path and filename of the source image

    commandShort[4] = FDD_CMD_UPLOADIMGBLOCK_START;             // starting image upload
    commandShort[5] = index;                                    // for this index

    p64kBlock       = pBfr;                                     // use this buffer for writing
    strcpy((char *) pBfr, fullPath);                            // and copy in the full path

    sectorCount     = 1;                                        // write just one sector

    BYTE res;
    res = Supexec(ce_acsiWriteBlockCommand);

    if(res == FDD_RES_ONDEVICECOPY) {                           // if the device returned this code, it means that it could do the image upload / copy on device, no need to upload it from ST!
        Fclose(fh);
        return;
    }

    if(res != FDD_OK) {                                         // bad? write error
        showComErrorDialog();

        Fclose(fh);                                             // close file and quit
        return;
    }
    //---------------
    // upload the image by 64kB blocks
    BYTE good = 1;
    BYTE blockNo = 0;

    sectorCount = 128;                                          // write 128 sectors (64 kB)
    int progress = 0;

    while(1) {
        showProgress(&dialog, progress);                        // update progress bar

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
}
