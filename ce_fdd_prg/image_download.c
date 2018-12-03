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
#include "aes.h"
#include "CE_FDD.H"

// ------------------------------------------------------------------
extern BYTE deviceID;
extern BYTE commandShort[CMD_LENGTH_SHORT];
extern BYTE commandLong [CMD_LENGTH_LONG];

extern BYTE *p64kBlock;
extern BYTE sectorCount;

extern BYTE *pBfr;

extern BYTE kbshift;

BYTE searchInit(void);

BYTE loopForDownload(void);
BYTE handleWriteSearch(BYTE key);
void showSearchString(void);
void showPageNumber(void);
void imageSearch(void);
void getResultsAndUpdatePageObjects(int page);
void showResults(void);
void selectDestinationDir(void);
BYTE refreshImageList(void);

void getStatus(BYTE alsoShow);

BYTE searchContent[2 * 512];        // currently shown content
BYTE prevSearchContent[2 * 512];    // previously shown content

#define MAX_SEARCHTEXT_LEN      12
struct {
    char    text[MAX_SEARCHTEXT_LEN + 1];
    int     len;

    int     pageCurrent;
    int     pagesCount;
} search;

Status status;

#define ROW_LENGTH  68

BYTE scrRez;

// ------------------------------------------------------------------

#define SHOWMENU_STATICTEXT     1
#define SHOWMENU_SEARCHSTRING   2
#define SHOWMENU_RESULTS_ALL    4
#define SHOWMENU_RESULTS_ROW    8

#define SHOWMENU_ALL            0xff

void insertPageRowIntoSlot(WORD page, BYTE row, BYTE slot);
void downloadPageRowToStorage(WORD page, BYTE row);

// ------------------------------------------------------------------

DWORD lastKeyPressTime;     // last time the key was pressed
BYTE searchNotApplied;      // set to TRUE when (after passing some time after last key press) should retrieve search results

DWORD lastStatusUpdate;     // last time when we checked and showed status

// ------------------------------------------------------------------
Dialog dialogDownload;              // dialog with image download content

#define WAITFOR_PRESSED     1
#define WAITFOR_RELEASED    0

// helper indices in otherObjs
#define EXIT2       0
#define LOADER      1
#define PAGE_PREV   2
#define PAGES       3
#define PAGE_NEXT   4
#define STATUS      5
#define SEARCH      6

#define ITEM_ROWS  8
const int16_t btnsDownloadNarrow[ITEM_ROWS] = { D0,  D1,  D2,  D3,  D4,  D5,  D6,  D7};
const int16_t btnsInsert1Narrow[ITEM_ROWS]  = {I01, I11, I21, I31, I41, I51, I61, I71};
const int16_t btnsInsert2Narrow[ITEM_ROWS]  = {I02, I12, I22, I32, I42, I52, I62, I72};
const int16_t btnsInsert3Narrow[ITEM_ROWS]  = {I03, I13, I23, I33, I43, I53, I63, I73};
const int16_t btnsContentNarrow[ITEM_ROWS]  = { C0,  C1,  C2,  C3,  C4,  C5,  C6,  C7};
const int16_t otherObjsNarrow[7] = {BTN_EXIT2, BTN_LOADER, BTN_PAGE_PREV, STR_PAGES, BTN_PAGE_NEXT, STR_STATUS, STR_SEARCH};

const int16_t btnsDownloadWide[ITEM_ROWS] = { D0W,  D1W,  D2W,  D3W,  D4W,  D5W,  D6W,  D7W};
const int16_t btnsInsert1Wide[ITEM_ROWS]  = {I01W, I11W, I21W, I31W, I41W, I51W, I61W, I71W};
const int16_t btnsInsert2Wide[ITEM_ROWS]  = {I02W, I12W, I22W, I32W, I42W, I52W, I62W, I72W};
const int16_t btnsInsert3Wide[ITEM_ROWS]  = {I03W, I13W, I23W, I33W, I43W, I53W, I63W, I73W};
const int16_t btnsContentWide[ITEM_ROWS]  = { C0W,  C1W,  C2W,  C3W,  C4W,  C5W,  C6W,  C7W};
const int16_t otherObjsWide[7] = {BTN_EXIT2W, BTN_LOADERW, BTN_PAGE_PREVW, STR_PAGESW, BTN_PAGE_NEXTW, STR_STATUSW, STR_SEARCHW};

const int16_t *btnsDownload;
const int16_t *btnsInsert1;
const int16_t *btnsInsert2;
const int16_t *btnsInsert3;
const int16_t *btnsContent;
const int16_t *otherObjs;

#define ROW_OBJ_HIDDEN      0
#define ROW_OBJ_VISIBLE     1
#define ROW_OBJ_SELECTED    2

void showResultsRow(int rowNo, char *content, char *prevContent)
{
    if(rowNo < 0 || rowNo >= ITEM_ROWS) {       // index out of range? quit
        return;
    }

    #define OBJS_IN_ROW     5
    int16_t i, idx[OBJS_IN_ROW];

    // get index for each button and string in this row
    idx[0] = btnsDownload[rowNo];
    idx[1] = btnsInsert1[rowNo];
    idx[2] = btnsInsert2[rowNo];
    idx[3] = btnsInsert3[rowNo];
    idx[4] = btnsContent[rowNo];

    #define IDX_OF_CONTENT      4
    #define CONTENT_OFFSET      5

    for(i=0; i<OBJS_IN_ROW; i++) {     // based on the ROW_OBJ_ flags for each button / string do hide, show, or select item
        if(idx[i] < 0 || idx[i] > cd->tree->ob_tail) {    // index too small or too big? skip it
            continue;
        }

        switch(content[i]) {
            // hidden object? set HIDE flag
            case ROW_OBJ_HIDDEN:    cd->tree[ idx[i] ].ob_flags |= OF_HIDETREE;
                                    break;

            // visible but not selected? remove HIDE flag, remove SELECTED state
            case ROW_OBJ_VISIBLE:   cd->tree[ idx[i] ].ob_flags &= (~OF_HIDETREE);
                                    cd->tree[ idx[i] ].ob_state &= (~OS_SELECTED);
                                    break;

            // visible and selected? remove HIDE flag, set SELECTED flag
            case ROW_OBJ_SELECTED:  cd->tree[ idx[i] ].ob_flags &= (~OF_HIDETREE);
                                    cd->tree[ idx[i] ].ob_state |= OS_SELECTED;
                                    break;
        }
        // if we're processing content string
        if(i == IDX_OF_CONTENT) {
            setObjectString( idx[i], content + CONTENT_OFFSET);
        }
    }
}

int16_t getIndexOfItem(const int16_t item, const int16_t *array)
{
    int i;
    for(i=0; i<ITEM_ROWS; i++) {
        if(array[i] == item) {          // if this item was found in the array, return index
            return i;
        }
    }

    return -1;          // not found, return -1
}

void getDownloadButtonRow(const int16_t btn, int *row)
{
    int idx = getIndexOfItem(btn, btnsDownload);    // try to get index
    *row = idx;     // 0-ITEM_ROWS if found, -1 if not found
}

void getInsertButtonRowAndSlot(const int16_t btn, int *row, int *slot)
{
    int idx;
    idx = getIndexOfItem(btn, btnsInsert1);

    if(idx != -1) { // if found in this column
        *slot = 1;
        *row = idx;
        return;
    }

    idx = getIndexOfItem(btn, btnsInsert2);

    if(idx != -1) { // if found in this column
        *slot = 2;
        *row = idx;
        return;
    }

    idx = getIndexOfItem(btn, btnsInsert3);

    if(idx != -1) { // if found in this column
        *slot = 3;
        *row = idx;
        return;
    }

    // if not found, return -1
    *slot = -1;
    *row = -1;
}

void enablePageButtons(void)
{
    enableButton(otherObjs[PAGE_PREV], search.pageCurrent != 0);   // enable PREV button if not 0th page
    enableButton(otherObjs[PAGE_NEXT], search.pageCurrent < (search.pagesCount -1));   // enable NEXT button if not last page
}

// handles page prev / page next click
void handlePrevNextPage(int16_t btn)
{
    if(btn == otherObjs[PAGE_PREV]) {      // prev page?
        if(search.pageCurrent > 0) {            // not the first page? move to previous page
            search.pageCurrent--;
            getResultsAndUpdatePageObjects(search.pageCurrent); // get previous results
        }
    }

    if(btn == otherObjs[PAGE_NEXT]) {      // next page?
        if(search.pageCurrent < (search.pagesCount - 1)) {  // not the last page? move to next page
           search.pageCurrent++;
           getResultsAndUpdatePageObjects(search.pageCurrent);  // get next results
        }
    }
}

void showSearchString(void)
{
    char tmp[32];
    strcpy(tmp, "Search: ");
    strcat(tmp, search.text);

    if(search.len < MAX_SEARCHTEXT_LEN) {   // if some char still can be added to search string
        int i;
        for(i=0; i<(MAX_SEARCHTEXT_LEN - search.len); i++) {    // add place holders
            strcat(tmp, "_");
        }
    }

    setObjectString(otherObjs[SEARCH], tmp);
}

BYTE unselectOldSelectNewImage(int newRow, int slot)
{
    const int16_t *btnsInsert;

    // get pointer to array which holds obj indexes for this slot (column of buttons)
    switch(slot) {
        case 1: btnsInsert = btnsInsert1; break;
        case 2: btnsInsert = btnsInsert2; break;
        case 3: btnsInsert = btnsInsert3; break;
        default: return FALSE;
    }

    int i, oldRow = -1;                 // no old row found yet
    for(i=0; i<ITEM_ROWS; i++) {        // try to find old (currently selected) button
        if(isSelected(btnsInsert[i])) { // this one is selected? good, store row and quit
            oldRow = i;
            break;
        }
    }

    if(oldRow != newRow) {              // if old row and new row are not the same
        selectButton(btnsInsert[oldRow], FALSE);    // unselect old
        selectButton(btnsInsert[newRow], TRUE);     // select new
        return TRUE;                    // selection changed
    }

    return FALSE;                       // selection not changed
}

void downloadHandleKeyPress(int16_t key)
{
    key = atariKeysToSingleByte(key >> 8, key);

    if(key >= 'A' && key <= 'Z') {  // upper case letter? to lower case!
        key += 32;
    }

    if((key >= 'a' && key <='z') || key == ' ' || (key >= 0 && key <= 9) || key == KEY_ESC || key == KEY_BACKSP) {
        BYTE changed = handleWriteSearch(key);

        if(changed) {                               // if the search string changed, search for images after some while
            lastKeyPressTime = getTicksAsUser();    // last time the key was pressed
            searchNotApplied = TRUE;                // should do imageSearch() soon
            showSearchString();
        }
    }
}

int16_t downloadHandleMouseButton(int16_t ev_mmbutton, int16_t ev_mmox, int16_t ev_mmoy, int16_t *btnWaitFor)
{
    int16_t exitobj = -1;
    static int16_t btnIsPressed = FALSE;    // current presset state

    if(btnIsPressed == FALSE) { // stored state = not pressed
        if(ev_mmbutton == 1) {   // button was pressed?
            btnIsPressed = TRUE;
            *btnWaitFor = WAITFOR_RELEASED;
        }
    } else {                    // stored state = is pressed
        if(ev_mmbutton == 0) {  // button was released?
            btnIsPressed = FALSE;
            *btnWaitFor = WAITFOR_PRESSED;

            exitobj = objc_find(dialogDownload.tree, ROOT, MAX_DEPTH, ev_mmox, ev_mmoy);   // find out index of object that was clicked
        }
    }

    return exitobj;
}

BYTE gem_imageDownload(void)
{
    search.len = 0;                                                 // clear search string
    memset(search.text, 0, MAX_SEARCHTEXT_LEN + 1);
    search.pageCurrent  = 0;
    search.pagesCount   = 0;

    scrRez = Getrez();                                              // get screen resolution into variable

    if(scrRez == 0) {   // low res - narrow dialog
        btnsDownload = btnsDownloadNarrow;
        btnsInsert1 = btnsInsert1Narrow;
        btnsInsert2 = btnsInsert2Narrow;
        btnsInsert3 = btnsInsert3Narrow;
        btnsContent = btnsContentNarrow;
        otherObjs = otherObjsNarrow;
    } else {            // other res - wide dialog
        btnsDownload = btnsDownloadWide;
        btnsInsert1 = btnsInsert1Wide;
        btnsInsert2 = btnsInsert2Wide;
        btnsInsert3 = btnsInsert3Wide;
        btnsContent = btnsContentWide;
        otherObjs = otherObjsWide;
    }

    BYTE res = searchInit();                                        // try to initialize

    if(res == 0) {                                                  // failed to initialize? return to floppy config screen
        return KEY_F9;
    }

    int16_t treeId = (scrRez == 0) ? DOWNLOAD : DOWNLOAD_WIDE;
    rsrc_gaddr(R_TREE, treeId, &dialogDownload.tree); // get address of dialog tree
    cd = &dialogDownload;   // set pointer to current dialog, so all helper functions will work with that dialog

    showDialog(TRUE);

    showSearchString();     // show the empty search string (if switched to already once opened form)
    imageSearch();          // get first results, fill it to dialog, drawing of dialog
    getStatus(TRUE);

    BYTE retVal = KEY_F10;

    int16_t btnWaitFor = WAITFOR_PRESSED;

    while(1) {
        int16_t msg_buf[8];
        int16_t dum, key, event_type;
        int16_t ev_mmox, ev_mmoy, ev_mmbutton;

        event_type = evnt_multi(MU_TIMER | MU_BUTTON | MU_KEYBD,   // int ev_mflags                             | short Type
            1, 1, btnWaitFor,// int ev_mbclicks, int ev_mbmask, int ev_mbstate,                                 | short Clicks, short WhichButton, short WhichState,
            0,0,0,0,0,      // int ev_mm1flags, int ev_mm1x, int ev_mm1y, int ev_mm1width, int ev_mm1height,    | short EnterExit1, short In1X, short In1Y, short In1W, short In1H,
            0,0,0,0,0,      // int ev_mm2flags, int ev_mm2x, int ev_mm2y, int ev_mm2width, int ev_mm2height,    | short EnterExit2, short In2X, short In2Y, short In2W, short In2H,
            msg_buf,        // int *ev_mmgpbuff,                                                                | short MesagBuf[],
            200,            // original TOS has this as 2 * int16_t, crossmint has this as unsigned long -- int ev_mtlocount, int ev_mthicount, | unsigned long Interval,
            &ev_mmox, &ev_mmoy, &ev_mmbutton,   // int *ev_mmox, int *ev_mmoy, int *ev_mmbutton,                | short *OutX, short *OutY, short *ButtonState
            &dum,           // int *ev_mmokstate,                                                               | short *KeyState,
            &key, &dum);    // int *ev_mkreturn, int *ev_mbreturn                                               | short *Key, short *ReturnCount

        if (event_type & MU_KEYBD) {    // on key pressed
            downloadHandleKeyPress(key);
        }

        if (event_type & MU_TIMER) {    // on timer, get status from device and show it
            DWORD now = getTicksAsUser();

            if((now - lastStatusUpdate) >= 200) {       // if typical period passed since the last status check, do it
                lastStatusUpdate = now;                 // we're getting status now
                BYTE refreshDataAndRedraw = FALSE;

                getStatus(TRUE);                        // talk to CE to see the status

                if(status.downloadCount == 0 && status.prevDownloadCount > 0) {     // if we just finished downloading (not downloading now, but were downloading a while ago)
                    refreshDataAndRedraw = TRUE;        // do a refresh
                }

                if(status.doWeHaveStorage != status.prevDoWeHaveStorage) {  // if user attached / detached drive, redraw all
                    refreshDataAndRedraw = TRUE;        // do a refresh
                }

                if(refreshDataAndRedraw) {              // should do a refresh?
                    getResultsAndUpdatePageObjects(search.pageCurrent); // refresh current page data
                }
            }
        }

        if(searchNotApplied) {                      // if we need to apply the search
            DWORD now = getTicksAsUser();           // get current time
            if((now - lastKeyPressTime) >= 100) {   // if enough time passed since the last key press, handle it
                searchNotApplied = FALSE;           // we're just handling this search
                imageSearch();
            }
        }

        int16_t exitobj = -1;           // no exit object yet

        if (event_type & MU_BUTTON) {   // left button event?
            exitobj = downloadHandleMouseButton(ev_mmbutton, ev_mmox, ev_mmoy, &btnWaitFor);
        }

        if(exitobj == -1) {     // if no exit object was specified, skip handling the rest
            continue;
        }

        if(exitobj == otherObjs[EXIT2]) {
            retVal = KEY_F10;   // KEY_F10 - quit
            break;
        }

        if(exitobj == otherObjs[LOADER]) {
            retVal = KEY_F9;   // KEY_F9 -- back to images loading / config dialog
            break;
        }

        if(exitobj == otherObjs[PAGE_PREV] || exitobj == otherObjs[PAGE_NEXT]) {  // prev/next page button pressed?
            handlePrevNextPage(exitobj);
            continue;
        }

        int row, slot;
        getDownloadButtonRow(exitobj, &row);    // was this download button press?

        if(row != -1) {         // handle download press
            status.downloadCount++;             // now downloading at least one thing
            lastStatusUpdate = getTicksAsUser(); // delay the status check a bit - fake that we've just retrieved the status

            selectButton(exitobj, TRUE);        // mark button as selected to tell user we're downloading it
            downloadPageRowToStorage(search.pageCurrent, row);
            continue;
        }

        getInsertButtonRowAndSlot(exitobj, &row, &slot);    // was this insert button press?

        if(row != -1) {             // handle insert press
            BYTE selectionChanged = unselectOldSelectNewImage(row, slot);

            if(selectionChanged) {  // insert only if selection changed
                insertPageRowIntoSlot(search.pageCurrent, row, slot - 1);
            }
            continue;
        }
    }

    showDialog(FALSE); // hide dialog
    return retVal;
}
// ------------------------------------------------------------------

BYTE refreshImageList(void)
{
    commandShort[4] = FDD_CMD_SEARCH_REFRESHLIST;                   // tell the host that it should refresh image list
    commandShort[5] = 0;

    sectorCount = 1;                                                // read 1 sector

    BYTE res = Supexec(ce_acsiReadCommand);

    if(res != FDD_OK) {
        showComErrorDialog();
        return 0;
    }

    res = searchInit();                                             // try to initialize
    return res;
}

void insertPageRowIntoSlot(WORD page, BYTE row, BYTE slot)
{
    if(!status.doWeHaveStorage) {                               // no storage? do nothing
        return;
    }

    commandShort[4] = FDD_CMD_SEARCH_INSERT2SLOT;
    commandShort[5] = 0;

    p64kBlock = pBfr;       // use this buffer for writing
    pBfr[0] = ITEM_ROWS;            // items per page
    pBfr[1] = (BYTE) (page >> 8);   // store page # high
    pBfr[2] = (BYTE) (page     );   // store page # low
    pBfr[3] = row;                  // store item #
    pBfr[4] = slot;                 // store slot #

    sectorCount = 1;                                            // write just one sector

    BYTE res = Supexec(ce_acsiWriteBlockCommand);

    if(res != FDD_OK) {                                         // bad? just be silent, CE_FDD.PRG doesn't know if this image is downloaded, so don't show warning
        return;
    }
}

void downloadPageRowToStorage(WORD page, BYTE row)
{
    if(!status.doWeHaveStorage) {                               // no storage? do nothing
        return;
    }

    commandShort[4] = FDD_CMD_SEARCH_DOWNLOAD2STORAGE;
    commandShort[5] = 0;

    p64kBlock = pBfr;                                           // use this buffer for writing
    pBfr[0] = ITEM_ROWS;            // items per page
    pBfr[1] = (BYTE) (page >> 8);   // store page # high
    pBfr[2] = (BYTE) (page     );   // store page # low
    pBfr[3] = row;                  // store item #

    sectorCount = 1;                                            // write just one sector

    BYTE res = Supexec(ce_acsiWriteBlockCommand);

    if(res != FDD_OK) {                                         // bad? write error
        showErrorDialog("Failed to start download to storage.\r\n");
        return;
    }

    getResultsAndUpdatePageObjects(search.pageCurrent);         // reload current page from host
}

void imageSearch(void)
{
    // first write the search string
    commandShort[4] = FDD_CMD_SEARCH_STRING;
    commandShort[5] = 0;

    p64kBlock       = pBfr;                                     // use this buffer for writing
    strcpy((char *) pBfr, search.text);                         // and copy in the search string

    sectorCount = 1;                                            // write just one sector

    BYTE res = Supexec(ce_acsiWriteBlockCommand);

	if(res != FDD_OK) {                                         // bad? write error
        showComErrorDialog();
        return;
    }

    getResultsAndUpdatePageObjects(0);
}

BYTE ce_acsiReadCommandLong(void);

void getResultsPage(int page)
{
    commandLong[5] = FDD_CMD_SEARCH_RESULTS;
    commandLong[6] = ITEM_ROWS;             // items per page
    commandLong[7] = (BYTE) (page >> 8);    // page high
    commandLong[8] = (BYTE) (page     );    // page low

    sectorCount = 2;                            // read 2 sectors
    BYTE res = Supexec(ce_acsiReadCommandLong);

    if(res != FDD_OK) {                         // bad? write error
        showComErrorDialog();
        return;
    }

    search.pageCurrent = BYTES_TO_INT(pBfr[0], pBfr[1]);   // get page #
    search.pagesCount  = BYTES_TO_INT(pBfr[2], pBfr[3]);   // get total page count

    memcpy(prevSearchContent, searchContent, ITEM_ROWS * ROW_LENGTH);   // remember previously shown content
    memcpy(searchContent, pBfr + 4, ITEM_ROWS * ROW_LENGTH);            // copy in new content
}

void getResultsAndUpdatePageObjects(int page)
{
    getResultsPage(page);           // get results from device

    cd->drawingDisabled = TRUE;     // disable partial redraws

    enablePageButtons();            // enable / disable page buttons
    showPageNumber();               // show page numbers
    showResults();                  // display results in dialog

    cd->drawingDisabled = FALSE;    // enable redrawing
    redrawDialog();                 // redraw dialog
}

BYTE handleWriteSearch(BYTE key)
{
    if(key == KEY_ESC) {                                                // esc - delete whole string
        search.len = 0;                                                 // clear search string
        memset(search.text, 0, MAX_SEARCHTEXT_LEN + 1);
        return 1;                                                       // search string changed
    }

    if(key == KEY_BACKSP) {                                             // backspace - delete single char
        if(search.len > 0) {
            search.len--;
            search.text[search.len] = 0;
            return 1;                                                   // search string changed
        }

        return 0;                                                       // search string NOT changed
    }

    if(search.len >= MAX_SEARCHTEXT_LEN) {                              // if the entered string is at maximum length
        return 0;                                                       // search string NOT changed
    }

    search.text[search.len] = key;                                      // add this key
    search.len++;                                                       // increase length
    search.text[search.len] = 0;                                        // terminate with zero

    return 1;                                                           // search string changed
}

void showResults(void)
{
    int i;

    for(i=0; i<ITEM_ROWS; i++) {
        char *pPrevRow    = (char *) (prevSearchContent + (i * ROW_LENGTH));
        char *pCurrentRow = (char *) (searchContent     + (i * ROW_LENGTH));

        showResultsRow(i, pCurrentRow, pPrevRow);
    }
}

void showPageNumber(void)
{
    BYTE pagesUiVisible = FALSE;    // assume that we don't have any results

    if(search.pagesCount != 0) {    // if got some results
        pagesUiVisible = TRUE;

        char tmp[12];
        intToStr(search.pageCurrent + 1, tmp);
        tmp[3] = '/';
        intToStr(search.pagesCount, tmp + 4);
        tmp[7] = 0;

        setObjectString(STR_PAGES, tmp);    // update string
    }

    // hide/show string and buttons
    setVisible(otherObjs[PAGES], pagesUiVisible);
    setVisible(otherObjs[PAGE_PREV], pagesUiVisible);
    setVisible(otherObjs[PAGE_NEXT], pagesUiVisible);
}

BYTE searchInit(void)
{
    commandShort[4] = FDD_CMD_SEARCH_INIT;
    commandShort[5] = scrRez;                   // screen resolution

    sectorCount = 1;                            // read 1 sector
    BYTE res;

    res = Supexec(ce_acsiReadCommand);

    if(res == FDD_DN_LIST) {
        showErrorDialog("Still downloading list of images...");
        return FALSE;       // can't show images yet
    } else if(res == FDD_ERROR) {
        showErrorDialog("Failed to initialize list.");
        return FALSE;
    } else if(res == FDD_OK) {
        return TRUE;        // everything OK
    }

    showComErrorDialog();   // some other error?
    return FALSE;
}

void getStatus(BYTE alsoShow)
{
    commandShort[4] = FDD_CMD_GET_IMAGE_ENCODING_RUNNING;
    commandShort[5] = 0;

    sectorCount = 1;                            // read 1 sector

    BYTE res = Supexec(ce_acsiReadCommand);

    if(res != FDD_OK) {                         // fail? just quit
        return;
    }

    status.encoding = pBfr[0];                  // isRunning - 1: is running, 0: is not running

    status.prevDoWeHaveStorage = status.doWeHaveStorage;    // make a copy of previous value
    status.doWeHaveStorage = pBfr[1];           // do we have storage on RPi attached?

    status.prevDownloadCount = status.downloadCount;        // make a copy of previous download files count
    status.downloadCount = pBfr[2];             // how many files are still downloading?

    if(alsoShow) {
        setObjectString(otherObjs[STATUS], (const char *) (pBfr + 4)); // show status line
    }
}
