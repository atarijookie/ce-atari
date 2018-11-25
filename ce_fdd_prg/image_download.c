#include <mint/osbind.h>
#include <gem.h>

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
void showMenuDownload(BYTE showMask);
BYTE handleWriteSearch(BYTE key);
void showSearchString(void);
void showPageNumber(void);
void imageSearch(void);
void getResultsPage(int page);
void showResults(BYTE showMask);
void selectDestinationDir(void);
BYTE refreshImageList(void);
void setSelectedRow(int row);

//void markCurrentRow(void);              // OBSOLETE
//void handleImagesDownload(void);        // OBSOLETE

void getStatus(void);
void insertCurrentIntoSlot(BYTE key);   // uses imageStorage
void downloadCurrentToStorage(void);    // uses imageStorage

BYTE searchContent[2 * 512];

#define MAX_SEARCHTEXT_LEN      20
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

// ------------------------------------------------------------------
Dialog dialogDownload;              // dialog with image download content

BYTE gem_imageDownload(void)
{
    rsrc_gaddr(R_TREE, DOWNLOAD, &dialogDownload.tree); // get address of dialog tree
    cd = &dialogDownload;           // set pointer to current dialog, so all helper functions will work with that dialog

    showDialog(TRUE);               // show dialog

    BYTE retVal = KEY_F10;

    while(1) {
        int16_t exitobj = form_do(dialogDownload.tree, 0) & 0x7FFF;

        if(exitobj == BTN_EXIT2) {
            retVal = KEY_F10;   // KEY_F10 - quit
            break;
        }

        if(exitobj == BTN_LOADER) {
            retVal = KEY_F9;   // KEY_F9 -- back to images loading / config dialog
            break;
        }

        // unselect button
        unselectButton(exitobj);

        if(exitobj == BTN_LOAD) {   // load image into slot

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
    showMenuDownload(SHOWMENU_ALL);

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
                showMenuDownload(SHOWMENU_ALL);                     // draw all on screen
            }
        }

        key = getKeyIfPossible();                                   // get key if one is waiting or just return 0 if no key is waiting

        if(key == 0) {                                              // no key? just go back to start
            continue;
        }

        kbshift = Kbshift(-1);

        if(key == KEY_F10 || key == KEY_F8) {                       // should quit or switch mode? 
            return key;
        }

        if(key == KEY_F1 || key == KEY_F2 || key == KEY_F3) {       // insert image into slot 1, 2, 3?
            insertCurrentIntoSlot(key);
            showMenuDownload(SHOWMENU_RESULTS_ROW);
            lastStatusCheckTime = 0;                                // force immediate status check and display
            continue;
        }

        if(key == KEY_F4) {                                         // start downloading images?
            downloadCurrentToStorage();                             // start download
            lastStatusCheckTime = 0;                                // force immediate status check and display
            continue;
        }

        if(key == KEY_F5) {                                         // refresh the list of images
            getResultsPage(search.pageCurrent);                     // refresh current page data
            showMenuDownload(SHOWMENU_ALL);                         // draw all on screen
            continue;
        }

        if(key >= 'A' && key <= 'Z') {								// upper case letter? to lower case!
            key += 32;
        }

        if((key >= 'a' && key <='z') || key == ' ' || (key >= 0 && key <= 9) || key == KEY_ESC || key == KEY_BACKSP) {
            res = handleWriteSearch(key);
            
            if(res) {                                               // if the search string changed, search for images
                imageSearch();

                showMenuMask = SHOWMENU_SEARCHSTRING | SHOWMENU_RESULTS_ALL;
            }
        }

        if((kbshift & (K_RSHIFT | K_LSHIFT)) != 0) {                // shift pressed? 
            if(key == KEY_PAGEUP) {                                 // shift + arrow up = page up
                gotoPrevPage = 1;
            }

            if(key == KEY_PAGEDOWN) {                               // shift + arrow down = page down
                gotoNextPage = 1;
            }
        } else {                                                    // shift not pressed?
            if(key == KEY_LEFT) {                                   // arrow left = prev page
                gotoPrevPage = 1;
            }

            if(key == KEY_RIGHT) {                                  // arrow right = next page
                gotoNextPage = 1;
            }

            if(key == KEY_UP) {                                     // arrow up?
                if(search.row > 0) {                                // not the top most line? move one line up
                    setSelectedRow(search.row - 1);
                    showMenuMask = SHOWMENU_RESULTS_ROW;            // just change the row highlighting
                } else {                                            // this is the top most line
                    gotoPrevPage = 1;
                }
            }

            if(key == KEY_DOWN) {                                   // arrow up?
                if(search.row < 14) {                               // not the bottom line? move one line down
                    setSelectedRow(search.row + 1);
                    showMenuMask = SHOWMENU_RESULTS_ROW;            // just change the row highlighting
                } else {                                            // this is the top most line
                    gotoNextPage = 1;
                }
            }
        }

        if(gotoPrevPage) {
            if(search.pageCurrent > 0) {                            // not the first page? move to previous page
                search.pageCurrent--;                               // 
                     
                getResultsPage(search.pageCurrent);                 // get previous results
                showMenuMask = SHOWMENU_RESULTS_ALL;                // redraw all results
                setSelectedRow(14);                                 // move to last row
            }
        }

        if(gotoNextPage) {
            if(search.pageCurrent < (search.pagesCount - 1)) {      // not the last page? move to next page
                search.pageCurrent++;                               // 
                     
                getResultsPage(search.pageCurrent);                 // get next results
                showMenuMask = SHOWMENU_RESULTS_ALL;                // redraw all results
                setSelectedRow(0);                                  // move to first row
            }
        }

        showMenuDownload(showMenuMask);
    }
}

BYTE refreshImageList(void)
{
    commandShort[4] = FDD_CMD_SEARCH_REFRESHLIST;                   // tell the host that it should refresh image list
    commandShort[5] = 0;

    sectorCount = 1;                                                // read 1 sector

    BYTE res = Supexec(ce_acsiReadCommand);

    if(res != FDD_OK) {
        showComError();
        return 0;
    }

    res = searchInit();                                             // try to initialize
    return res;
}

void insertCurrentIntoSlot(BYTE key)
{
    if(!status.doWeHaveStorage) {                               // no storage? do nothing
        return;
    }

    commandShort[4] = FDD_CMD_SEARCH_INSERT2SLOT;
    commandShort[5] = 0;

    p64kBlock = pBfr;                                           // use this buffer for writing
    pBfr[0] = search.pageCurrent;                               // store page #
    pBfr[1] = search.row;                                       // store item #
    pBfr[2] = key - KEY_F1;                                     // slot number - transform F1-F3 to 0-2

    sectorCount = 1;                                            // write just one sector

    BYTE res = Supexec(ce_acsiWriteBlockCommand);

    if(res != FDD_OK) {                                         // bad? just be silent, CE_FDD.PRG doesn't know if this image is downloaded, so don't show warning
        return;
    }

    getResultsPage(search.pageCurrent);                         // reload current page from host
}

void downloadCurrentToStorage(void)
{
    if(!status.doWeHaveStorage) {                               // no storage? do nothing
        return;
    }

    commandShort[4] = FDD_CMD_SEARCH_DOWNLOAD2STORAGE;
    commandShort[5] = 0;

    p64kBlock = pBfr;                                           // use this buffer for writing
    pBfr[0] = search.pageCurrent;                               // store page #
    pBfr[1] = search.row;                                       // store item #
    
    sectorCount = 1;                                            // write just one sector
    
    BYTE res = Supexec(ce_acsiWriteBlockCommand); 
    
    if(res != FDD_OK) {                                         // bad? write error
        showError("Failed to start download to storage.\r\n");
        return;
    }

    getResultsPage(search.pageCurrent);                         // reload current page from host
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
        showComError();
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
        showComError();
        return;
    }

    search.pageCurrent  = (int) pBfr[0];        // get page #
    search.pagesCount   = (int) pBfr[1];        // get total page count

    memcpy(searchContent, pBfr + 2, 15 * ROW_LENGTH);   // copy the data
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

void addPaddingIfNeeded(void)
{
    if(scrRez) {                                // on mid/high res
        (void) Cconws("                    ");  // add extra spaces
    }
}

void showMenuDownload(BYTE showMask)
{
    if(showMask & SHOWMENU_STATICTEXT) {
        (void) Clear_home();
        (void) Cconws("\33p");
        addPaddingIfNeeded();
        (void) Cconws("[Floppy image download,  Jookie 2014-18]");
        addPaddingIfNeeded();
        (void) Cconws("\33q\r\n");
    }

    if(showMask & SHOWMENU_SEARCHSTRING) {
        showSearchString();
    }

    if(showMask & SHOWMENU_RESULTS_ALL) {
        showPageNumber();
    }

    showResults(showMask);

    if(showMask & SHOWMENU_STATICTEXT) {
        Goto_pos(0, 19);

        addPaddingIfNeeded();
        (void) Cconws("\33pA..Z\33q search          \33parrows\33q move\r\n");

        if(status.doWeHaveStorage) {    // with storage
            addPaddingIfNeeded();
            (void) Cconws("\33pF1, F2, F3\33q -> insert into slot 1, 2, 3\r\n");

            addPaddingIfNeeded();
            (void) Cconws("\33pF4\33q   download        \33pF5\33q  refresh list\r\n");
        } else {                        // without storage
            addPaddingIfNeeded();
            (void) Cconws("                                         \r\n");

            addPaddingIfNeeded();
            (void) Cconws("                     \33pF5\33q  refresh list\r\n");
        }

        addPaddingIfNeeded();
        (void) Cconws("\33pF8\33q   setup screen    \33pF10\33q quit\r\n");
    }
}

void showResults(BYTE showMask)
{
    int i;
    char *pRow;

    if(showMask & SHOWMENU_RESULTS_ALL) {               // if should redraw all results
        (void) Cconws("\33w");                          // disable line wrap

        for(i=0; i<15; i++) {
            pRow = (char *) (searchContent + (i * ROW_LENGTH));

            BYTE selected = (i == search.row);

            Goto_pos(0, 3 + i);
            (void) Cconws("\33K");                      // clear line from cursor to right

            if(selected) {                              // for selected row
                (void) Cconws("\33p");
            }

            (void) Cconws(pRow);

            if(selected) {                              // for selected row
                (void) Cconws("\33q");
            }
        }

        (void) Cconws("\33v");                          // enable line wrap
        return;
    }

    if(showMask & SHOWMENU_RESULTS_ROW) {               // if should redraw only selected line
        (void) Cconws("\33w");                          // disable line wrap

        // draw previous line without inversion
        Goto_pos(0, 3 + search.prevRow);

        pRow = (char *) (searchContent + (search.prevRow * ROW_LENGTH));
        (void) Cconws(pRow);

        // draw current line with inversion
        Goto_pos(0, 3 + search.row);

        pRow = (char *) (searchContent + (search.row * ROW_LENGTH));

        (void) Cconws("\33p");
        (void) Cconws(pRow);                            // show current but altered row
        (void) Cconws("\33q");

        (void) Cconws("\33v");                          // enable line wrap

        return;
    }
}

void showPageNumber(void)
{
    Goto_pos(19, 2);
    (void) Cconws("\33pPage  :\33q        ");
    Goto_pos(27, 2);

    char tmp[10];
    intToStr(search.pageCurrent + 1, tmp);
    tmp[3] = '/';
    intToStr(search.pagesCount, tmp + 4);
    (void) Cconws(tmp);
}

void showSearchString(void)
{
    Goto_pos(19, 1);
    (void) Cconws("\33pSearch:\33q             ");
    Goto_pos(27, 1);
    (void) Cconws(search.text);
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
            (void) Cconws("CosmosEx device communication problem!\n\r");
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

    Goto_pos(0, 23);                            // show status line
    (void) Cconws("\33p");                      // inverse on
    addPaddingIfNeeded();
    (void) Cconws((const char *) (pBfr + 4));   // status string
    addPaddingIfNeeded();
    (void) Cconws("\33q");                      // inverse off
}
