#include <mint/osbind.h>
#include <gem.h>
#include <mt_gem.h>

#include "acsi.h"
#include "main.h"
#include "hostmoddefs.h"
#include "keys.h"
#include "defs.h"
#include "stdlib.h"
#include "aes.h"
#include "CE_FDD.H"

// ------------------------------------------------------------------
extern BYTE deviceID;
extern BYTE commandShort[CMD_LENGTH_SHORT];

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
void getResultsPage(int page);
void showResults(void);
void selectDestinationDir(void);
BYTE refreshImageList(void);
void setSelectedRow(int row);

void getStatus(void);
void insertCurrentIntoSlot(BYTE key);   // uses imageStorage
void downloadCurrentToStorage(void);    // uses imageStorage

BYTE searchContent[2 * 512];

#define MAX_SEARCHTEXT_LEN      12
struct {
    char    text[MAX_SEARCHTEXT_LEN + 1];
    int     len;

    int     pageCurrent;
    int     pagesCount;

    int     row;
    int     prevRow;
} search;

struct {
    BYTE encoding;              // is the RPi encoding the image or being idle?
    BYTE doWeHaveStorage;       // do we have storage for floppy images?
    BYTE prevDoWeHaveStorage;   // previous value of doWeHaveStorage

    BYTE downloadCount;         // how many files are now being downloaded?
    BYTE prevDownloadCount;     // previous value of downloadCount
} status;

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
Dialog dialogDownload;              // dialog with image download content

#define WAITFOR_PRESSED     1
#define WAITFOR_RELEASED    0

#define ITEM_ROWS  8
const int16_t btnsDownload[ITEM_ROWS] = { D0,  D1,  D2,  D3,  D4,  D5,  D6,  D7};
const int16_t btnsInsert1[ITEM_ROWS]  = {I01, I11, I21, I31, I41, I51, I61, I71};
const int16_t btnsInsert2[ITEM_ROWS]  = {I02, I12, I22, I32, I42, I52, I62, I72};
const int16_t btnsInsert3[ITEM_ROWS]  = {I03, I13, I23, I33, I43, I53, I63, I73};
const int16_t btnsContent[ITEM_ROWS]  = { C0,  C1,  C2,  C3,  C4,  C5,  C6,  C7};

#define ROW_OBJ_HIDDEN      0
#define ROW_OBJ_VISIBLE     1
#define ROW_OBJ_SELECTED    2

void showResultsRow(int rowNo, char *content)
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

    for(i=0; i<OBJS_IN_ROW; i++) {  // based on the ROW_OBJ_ flags for each button / string do hide, show, or select item
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

        // if we're processing content object and it's not hidden, set also new string
        if(i == 4 && content[4] != ROW_OBJ_HIDDEN) {
            setObjectString( idx[i], content + 5);
        }

        // now redraw object
        redrawObject( idx[i] );
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

void handlePrevNextPage(int16_t btn)
{
    if(btn == BTN_PAGE_PREV) {      // prev page?
        if(search.pageCurrent > 0) {            // not the first page? move to previous page
            search.pageCurrent--;
//            getResultsPage(search.pageCurrent); // get previous results
        }
    }

    if(btn == BTN_PAGE_NEXT) {      // next page?
        if(search.pageCurrent < (search.pagesCount - 1)) {  // not the last page? move to next page
           search.pageCurrent++;
//           getResultsPage(search.pageCurrent);  // get next results
        }
    }

    enableButton(BTN_PAGE_PREV, search.pageCurrent != 0);   // enable PREV button if not 0th page
    enableButton(BTN_PAGE_NEXT, search.pageCurrent < (search.pagesCount -1));   // enable NEXT button if not last page

    showPageNumber();
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

    setObjectString(STR_SEARCH, tmp);
}

void downloadHandleKeyPress(int16_t key)
{
    key = atariKeysToSingleByte(key >> 8, key);

    if(key >= 'A' && key <= 'Z') {  // upper case letter? to lower case!
        key += 32;
    }

    if((key >= 'a' && key <='z') || key == ' ' || (key >= 0 && key <= 9) || key == KEY_ESC || key == KEY_BACKSP) {
        BYTE changed = handleWriteSearch(key);

        if(changed) {               // if the search string changed, search for images
            showSearchString();
            // imageSearch();
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
    search.row          = 0;
    search.prevRow      = 0;

    scrRez = Getrez();                                              // get screen resolution into variable

    rsrc_gaddr(R_TREE, DOWNLOAD, &dialogDownload.tree); // get address of dialog tree
    cd = &dialogDownload;           // set pointer to current dialog, so all helper functions will work with that dialog

    showDialog(TRUE);               // show dialog

    showResults();

    BYTE retVal = KEY_F10;

    int16_t btnWaitFor = WAITFOR_PRESSED;

    while(1) {
        // TODO: get and show status (limit to chars)
        // TODO: send PAGE to RPi as WORD (now it's BYTE)
        // TODO: change page size for PRG retrieving to 8 items (now it's 15)

        int16_t msg_buf[8];
        int16_t dum, key, event_type;
        int16_t ev_mmox, ev_mmoy, ev_mmbutton;

        event_type = evnt_multi(MU_TIMER | MU_BUTTON | MU_KEYBD,   // int ev_mflags                             | short Type
            1, 1, btnWaitFor,// int ev_mbclicks, int ev_mbmask, int ev_mbstate,                                 | short Clicks, short WhichButton, short WhichState,
            0,0,0,0,0,      // int ev_mm1flags, int ev_mm1x, int ev_mm1y, int ev_mm1width, int ev_mm1height,    | short EnterExit1, short In1X, short In1Y, short In1W, short In1H,
            0,0,0,0,0,      // int ev_mm2flags, int ev_mm2x, int ev_mm2y, int ev_mm2width, int ev_mm2height,    | short EnterExit2, short In2X, short In2Y, short In2W, short In2H,
            msg_buf,        // int *ev_mmgpbuff,                                                                | short MesagBuf[],
            1000,           // original TOS has this as 2 * int16_t, crossmint has this as unsigned long -- int ev_mtlocount, int ev_mthicount, | unsigned long Interval,
            &ev_mmox, &ev_mmoy, &ev_mmbutton,   // int *ev_mmox, int *ev_mmoy, int *ev_mmbutton,                | short *OutX, short *OutY, short *ButtonState
            &dum,           // int *ev_mmokstate,                                                               | short *KeyState,
            &key, &dum);    // int *ev_mkreturn, int *ev_mbreturn                                               | short *Key, short *ReturnCount

        if (event_type & MU_KEYBD) {    // on key pressed
            downloadHandleKeyPress(key);
        }

        if (event_type & MU_TIMER) {    // on timer, get status from device and show it
//          getStatus();                // talk to CE to see the status
        }

        int16_t exitobj = -1;           // no exit object yet

        if (event_type & MU_BUTTON) {   // left button event?
            exitobj = downloadHandleMouseButton(ev_mmbutton, ev_mmox, ev_mmoy, &btnWaitFor);
        }

        if(exitobj == -1) {     // if no exit object was specified, skip handling the rest
            continue;
        }

        if(exitobj == BTN_EXIT2) {
            retVal = KEY_F10;   // KEY_F10 - quit
            break;
        }

        if(exitobj == BTN_LOADER) {
            retVal = KEY_F9;   // KEY_F9 -- back to images loading / config dialog
            break;
        }

        if(exitobj == BTN_PAGE_PREV || exitobj == BTN_PAGE_NEXT) {  // prev/next page button pressed?
            handlePrevNextPage(exitobj);
            continue;
        }

        int row, slot;
        getDownloadButtonRow(exitobj, &row);    // was this download button press?

        if(row != -1) {         // handle download press
            selectButton(exitobj, TRUE);        // mark button as selected to tell user we're downloading it 
            //downloadPageRowToStorage(search.pageCurrent, row);
            continue;
        }

        getInsertButtonRowAndSlot(exitobj, &row, &slot);    // was this insert button press?

        if(row != -1) {         // handle insert press
            //insertPageRowIntoSlot(search.pageCurrent, row, slot);
            continue;
        }
    }

    showDialog(FALSE); // hide dialog
    return retVal;
}
// ------------------------------------------------------------------

BYTE loopForDownload(void)
{
    DWORD lastStatusCheckTime = 0;

    search.len = 0;                                                 // clear search string
    memset(search.text, 0, MAX_SEARCHTEXT_LEN + 1);
    search.pageCurrent  = 0;
    search.pagesCount   = 0;
    search.row          = 0;
    search.prevRow      = 0;

    scrRez = Getrez();                                              // get screen resolution into variable

    BYTE res = searchInit();                                        // try to initialize

    if(res == 0) {                                                  // failed to initialize? return to floppy config screen
        return KEY_F8;
    }

    getStatus();
    lastStatusCheckTime = getTicksAsUser();                         // fill status.doWeHaveStorage before 1st showMenuDownload()

    imageSearch();

    BYTE showMenuMask;
    BYTE gotoPrevPage, gotoNextPage;

    while(1) {
        BYTE key, kbshift;
        DWORD now = getTicksAsUser();

        gotoPrevPage = 0;
        gotoNextPage = 0;
        showMenuMask = 0;

        if(now >= (lastStatusCheckTime + 100)) {                    // if last check was at least a while ago, do new check
            BYTE refreshDataAndRedraw = FALSE;

            lastStatusCheckTime = now;
            getStatus();                                            // talk to CE to see the status

            if(status.downloadCount == 0 && status.prevDownloadCount > 0) {     // if we just finished downloading (not downloading now, but were downloading a while ago)
                refreshDataAndRedraw = TRUE;                            // do a refresh
            }

            if(status.doWeHaveStorage != status.prevDoWeHaveStorage) {  // if user attached / detached drive, redraw all
                refreshDataAndRedraw = TRUE;                            // do a refresh
            }

            if(refreshDataAndRedraw) {                              // should do a refresh?
                getResultsPage(search.pageCurrent);                 // refresh current page data
            }
        }

        key = getKeyIfPossible();                                   // get key if one is waiting or just return 0 if no key is waiting

        if(key == 0) {                                              // no key? just go back to start
            continue;
        }

        kbshift = Kbshift(-1);
    }
}

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
    pBfr[0] = page;         // store page #
    pBfr[1] = row;          // store item #
    pBfr[2] = slot;         // store slot #

    sectorCount = 1;                                            // write just one sector

    BYTE res = Supexec(ce_acsiWriteBlockCommand);

    if(res != FDD_OK) {                                         // bad? just be silent, CE_FDD.PRG doesn't know if this image is downloaded, so don't show warning
        return;
    }

    getResultsPage(search.pageCurrent);                         // reload current page from host
}

void insertCurrentIntoSlot(BYTE key)
{
    insertPageRowIntoSlot(search.pageCurrent, search.row, key - KEY_F1);
}

void downloadPageRowToStorage(WORD page, BYTE row)
{
    if(!status.doWeHaveStorage) {                               // no storage? do nothing
        return;
    }

    commandShort[4] = FDD_CMD_SEARCH_DOWNLOAD2STORAGE;
    commandShort[5] = 0;

    p64kBlock = pBfr;                                           // use this buffer for writing
    pBfr[0] = page;                               // store page #
    pBfr[1] = row;                                // store item #

    sectorCount = 1;                                            // write just one sector

    BYTE res = Supexec(ce_acsiWriteBlockCommand);

    if(res != FDD_OK) {                                         // bad? write error
        showError("Failed to start download to storage.\r\n");
        return;
    }

    getResultsPage(search.pageCurrent);                         // reload current page from host
}

void downloadCurrentToStorage(void)
{
    downloadPageRowToStorage(search.pageCurrent, search.row);
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

    search.row      = 0;
    search.prevRow  = 0;

    getResultsPage(0);
}

void getResultsPage(int page)
{
    commandShort[4] = FDD_CMD_SEARCH_RESULTS;
    commandShort[5] = (BYTE) page;

    sectorCount = 2;                            // read 2 sectors
    BYTE res = Supexec(ce_acsiReadCommand);

    if(res != FDD_OK) {                         // bad? write error
        showComErrorDialog();
        return;
    }

    search.pageCurrent  = (int) pBfr[0];        // get page #
    search.pagesCount   = (int) pBfr[1];        // get total page count

    memcpy(searchContent, pBfr + 2, ITEM_ROWS * ROW_LENGTH);   // copy the data
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
    char *pRow;

    for(i=0; i<ITEM_ROWS; i++) {
        pRow = (char *) (searchContent + (i * ROW_LENGTH));
        showResultsRow(i, pRow);
    }
}

void showPageNumber(void)
{
    char tmp[10];
    intToStr(search.pageCurrent + 1, tmp);
    tmp[3] = '/';
    intToStr(search.pagesCount, tmp + 4);
    tmp[7] = 0;

    setObjectString(STR_PAGES, tmp);    // update string
}

void setSelectedRow(int row)
{
    search.prevRow  = search.row;           // store current line as previous one
    search.row      = row;                  // store new line as the current one
}

BYTE searchInit(void)
{
    commandShort[4] = FDD_CMD_SEARCH_INIT;
    commandShort[5] = scrRez;                   // screen resolution

    sectorCount = 1;                            // read 1 sector
    BYTE res;

    (void) Clear_home();
    (void) Cconws("Initializing... Press ESC to stop.\n\r\n\r");

    while(1) {
        res = Supexec(ce_acsiReadCommand);

        if(res == FDD_DN_LIST) {
            (void) Cconws("Downloading list of floppy images...\n\r");
        } else if(res == FDD_ERROR) {
            (void) Cconws("Failed to initialize! Press any key.\n\r");
            Cnecin();
            return 0;
        } else if(res == FDD_OK) {
            (void) Cconws("Done.\n\r");
            return 1;
        } else {
            showComErrorDialog();
        }

        WORD val = Cconis();            // see if there is some char waiting
        if(val != 0) {                  // char waiting?
            BYTE key = getKey();

            if(key == KEY_ESC) {
                (void) Cconws("Init terminated by user. Press any key.\n\r");
                Cnecin();
                return 0;
            }
        }

        sleep(1);
    }
}

void getStatus(void)
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

    setObjectString(STR_STATUS, (const char *) (pBfr + 4)); // show status line
}
