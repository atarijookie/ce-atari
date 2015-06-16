#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <unistd.h>
#include <gem.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "acsi.h"
#include "main.h"
#include "hostmoddefs.h"
#include "keys.h"
#include "defs.h"
       
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
void setSelectedRow(int row);
void markCurrentRow(void);
void handleImagesDownload(void);
void selectDestinationDir(void);
BYTE refreshImageList(void);

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

TDestDir destDir;

void downloadImage(int index);

// ------------------------------------------------------------------ 

#define SHOWMENU_STATICTEXT     1
#define SHOWMENU_SEARCHSTRING   2
#define SHOWMENU_RESULTS_ALL    4
#define SHOWMENU_RESULTS_ROW    8

#define SHOWMENU_ALL            0xff

BYTE loopForDownload(void)
{
    search.len = 0;                                                 // clear search string
    memset(search.text, 0, MAX_SEARCHTEXT_LEN + 1);
    search.pageCurrent  = 0;
    search.pagesCount   = 0;
    search.row          = 0;
    search.prevRow      = 0;

    destDir.isSet = 0;
    
    BYTE res = searchInit();                                        // try to initialize
    
    if(res == 0) {                                                  // failed to initialize? return to floppy config screen
        return KEY_F8;
    }
    
    imageSearch();
    showMenuDownload(SHOWMENU_ALL);

    BYTE showMenuMask;
    BYTE gotoPrevPage, gotoNextPage;
    
    while(1) {
        BYTE key, kbshift;
        
        gotoPrevPage = 0;
        gotoNextPage = 0;
        showMenuMask = 0;
        
        key     = getKey();
        kbshift = Kbshift(-1);
        
        if(key == KEY_F10 || key == KEY_F8) {                       // should quit or switch mode? 
            return key;
        }
        
        if(key == KEY_F1) {                                         // mark current row? 
            markCurrentRow();
            showMenuMask = SHOWMENU_RESULTS_ROW;
        }

        if(key == KEY_F2) {                                         // select destination directory?
            selectDestinationDir();
            showMenuMask = SHOWMENU_ALL;
        }
        
        if(key == KEY_F3) {                                         // start downloading images?
            handleImagesDownload();
            getResultsPage(search.pageCurrent);                     // refresh current page (will unselect the selected images)
            showMenuMask = SHOWMENU_ALL;
        }

        if(key == KEY_F5) {                                         // refresh the list of images
            res = refreshImageList();
            
            if(res == 0) {                                          // failed? switch to config screen
                return KEY_F8;
            }
            
            showMenuMask = SHOWMENU_ALL;
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

void selectDestinationDir(void)
{
    short button;
    char path[256], fname[256];

    memset(path,    0, 256);
    memset(fname,   0, 256);
    
    strcpy(path, "C:\\*.*");                            
    BYTE drive = getLowestDrive();              // get the lowest HDD letter and use it in the file selector
    path[0] = drive;
    
    graf_mouse(M_ON, 0);
    fsel_input(path, fname, &button);           // show file selector
    graf_mouse(M_OFF, 0);
  
    if(button != 1) {                           // if OK was not pressed
        return;
    }

    removeLastPartUntilBackslash(path);         // remove part behind the last separator (most probably wild card)

    destDir.isSet = 1;
    strcpy(destDir.path, path);                 // copy in the destination dir
}

void handleImagesDownload(void)
{
    if(destDir.isSet == 0) {                    // destination dir not set?
        selectDestinationDir();
        
        if(destDir.isSet == 0) {                // destination dir still not set?
            (void) Clear_home();
            (void) Cconws("You have to select destination dir!\r\nPress any key to continue.\n\r");
            getKey();
            return;
        }
    }
    
    BYTE res;
    
    (void) Clear_home();
    (void) Cconws("Downloading selected images...\r\n");

    while(1) {
        commandShort[4] = FDD_CMD_SEARCH_DOWNLOAD;
        commandShort[5] = 0;
        sectorCount = 1;                        // read 1 sector
        res = Supexec(ce_acsiReadCommand);
		
        if(res == FDD_DN_WORKING) {             // if downloading
            (void) Cconws(pBfr);                // write out status string
            (void) Cconws("\n\r");
        } else if(res == FDD_DN_NOTHING_MORE) { // if nothing more to download
            (void) Cconws("All selected images downloaded.\r\nPress any key to continue.\n\r");
            getKey();
            break;
        } else if(res == FDD_DN_DONE) {         // if this image finished downloading
            downloadImage(10);                  // store this downloaded image
            (void) Cconws("\n\r\n\r");
        } 

        sleep(1);                               // wait a second
    }
}

void markCurrentRow(void)
{
    commandShort[4] = FDD_CMD_SEARCH_MARK;
    commandShort[5] = 0;

    p64kBlock = pBfr;                                           // use this buffer for writing
    pBfr[0] = search.pageCurrent;                               // store page #
    pBfr[1] = search.row;                                       // store item #
    
    sectorCount = 1;                                            // write just one sector
    
    BYTE res = Supexec(ce_acsiWriteBlockCommand); 
    
	if(res != FDD_OK) {                                         // bad? write error
        showComError();
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

    memcpy(searchContent, pBfr + 2, 15 * 68);   // copy the data
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

void showMenuDownload(BYTE showMask)
{
    if(showMask & SHOWMENU_STATICTEXT) {
        (void) Clear_home();
        (void) Cconws("\33p[CosmosEx image download by Jookie 2014]\33q\r\n");
    }
	
    if(showMask & SHOWMENU_SEARCHSTRING) {
        showSearchString();
    }
    
    if(showMask & SHOWMENU_RESULTS_ALL) {
        showPageNumber();
    }
    
    showResults(showMask);
    
    if(showMask & SHOWMENU_STATICTEXT) {
        Goto_pos(0, 20);
        (void) Cconws("\33pA..Z\33q - search, \33p(shift) arrows\33q - move\r\n");
        (void) Cconws("\33pF1\33q   - mark,         \33pF2\33q  - dest. dir,\r\n");
        (void) Cconws("\33pF3\33q   - download,     \33pF5\33q  - refresh list,\r\n");
        (void) Cconws("\33pF8\33q   - setup screen, \33pF10\33q - quit\r\n");
    }
}

void showResults(BYTE showMask)
{
    int i;
    char *pRow;

    if(showMask & SHOWMENU_RESULTS_ALL) {               // if should redraw all results
        (void) Cconws("\33w");                          // disable line wrap
    
        for(i=0; i<15; i++) {
            pRow = (char *) (searchContent + (i * 68));

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

        pRow = (char *) (searchContent + (search.prevRow * 68));
        (void) Cconws(pRow);

        // draw current line with inversion
        Goto_pos(0, 3 + search.row);

        pRow = (char *) (searchContent + (search.row * 68));
        
        (void) Cconws("\33p");
        (void) Cconws(pRow);
        (void) Cconws("\33q");
        
        (void) Cconws("\33v");                          // enable line wrap

        return;
    }
}

void showPageNumber(void)
{
    Goto_pos(13, 2);
    (void) Cconws("Page:        ");
    Goto_pos(19, 2);

    char tmp[10];
    intToStr(search.pageCurrent, tmp);
    tmp[3] = '/';
    intToStr(search.pagesCount, tmp + 4);
    (void) Cconws(tmp);
}

void showSearchString(void)
{
    Goto_pos(0, 1);
    (void) Cconws("Search:                              ");
    Goto_pos(8, 1);
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
    commandShort[5] = 0;
    
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
