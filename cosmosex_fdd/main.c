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
       
// ------------------------------------------------------------------ 
BYTE ce_findId(void);
BYTE ce_identify(void);

BYTE deviceID;

BYTE commandShort[CMD_LENGTH_SHORT]	= {	0, 'C', 'E', HOSTMOD_FDD_SETUP, 0, 0};

BYTE siloContent[512];

void uploadImage(int index);
void swapImage(int index);
void removeImage(int index);

BYTE getLowestDrive(void);
void removeLastPartUntilBackslash(char *str);

BYTE ce_acsiReadCommand(void);
BYTE ce_acsiWriteBlockCommand(void);

void newImage(int index);
void downloadImage(int index);

void showComError(void);
void createFullPath(char *fullPath, char *filePath, char *fileName);

char filePath[256], fileName[256];

BYTE *p64kBlock;
BYTE sectorCount;

#define GOTO_POS        "\33Y"
#define Goto_pos(x,y)   ((void) Cconws(GOTO_POS),  (void) Cconout(' ' + y), (void) Cconout(' ' + x))

BYTE nextMenuRedrawIsFull;

#define SIZE64K     (64*1024)
BYTE *pBfrOrig;
BYTE *pBfr, *pBfrCnt;

BYTE getKey(void);
BYTE atariKeysToSingleByte(BYTE vkey, BYTE key);

BYTE loopForSetup(void);
void showMenu(char fullNotPartial);
void showImage(int index);
void getSiloContent(void);

BYTE loopForDownload(void);
void showMenuDownload(BYTE showMask);
BYTE handleWriteSearch(BYTE key);
void showSearchString(void);
void showPageNumber(void);
void intToStr(int val, char *str);
void imageSearch(void);
void getResultsPage(int page);
void showResults(BYTE showMask);
void setSelectedRow(int row);
void markCurrentRow(void);

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

// ------------------------------------------------------------------ 
int main( int argc, char* argv[] )
{
	BYTE found;
  
    appl_init();										            // init AES 
    
	pBfrOrig = (BYTE *) Malloc(SIZE64K + 4);
	
	if(pBfrOrig == NULL) {
		(void) Cconws("\r\nMalloc failed!\r\n");
		sleep(3);
		return 0;
	}

	DWORD val = (DWORD) pBfrOrig;
	pBfr      = (BYTE *) ((val + 4) & 0xfffffffe);     				// create even pointer
    pBfrCnt   = pBfr - 2;											// this is previous pointer - size of WORD 
	
    // init fileselector path
    strcpy(filePath, "C:\\*.*");                            
    memset(fileName, 0, 256);          
    
    BYTE drive = getLowestDrive();                                  // get the lowest HDD letter and use it in the file selector
    filePath[0] = drive;
    
	// write some header out
	(void) Clear_home();
	(void) Cconws("\33p[ CosmosEx floppy setup ]\r\n[    by Jookie 2014     ]\33q\r\n\r\n");

	// search for CosmosEx on ACSI bus
/*    
	found = ce_findId();
	if(!found) {								                    // not found? quit
		sleep(3);
		return 0;
	}
*/
deviceID=0;

	// now set up the acsi command bytes so we don't have to deal with this one anymore 
	commandShort[0] = (deviceID << 5); 					            // cmd[0] = ACSI_id + TEST UNIT READY (0)	

	graf_mouse(M_OFF, 0);
	

    while(1) {
        BYTE key;
        
        key = loopForSetup();
        
        if(key == KEY_F10) {                // should quit?
            break;
        }
    
        key = loopForDownload();
    
        if(key == KEY_F10) {                // should quit?
            break;
        }
    }    
    
	graf_mouse(M_ON, 0);
	appl_exit();

	Mfree(pBfrOrig);
	
	return 0;		
}

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

    imageSearch();
    showMenuDownload(SHOWMENU_ALL);

    BYTE showMenuMask;
    BYTE gotoPrevPage, gotoNextPage;
    
    while(1) {
        BYTE key, res;
        BYTE kbshift;
        
        gotoPrevPage = 0;
        gotoNextPage = 0;
        showMenuMask = 0;
        
        key     = getKey();
        kbshift = Kbshift(-1);                                      // get shift status

        if(key == KEY_F10 || key == KEY_F8) {                       // should quit or switch mode? 
            return key;
        }
        
        if(key == KEY_F1) {                                         // mark current row? 
            markCurrentRow();
            showMenuMask = SHOWMENU_RESULTS_ROW;
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
            if(key == KEY_UP) {                                     // shift + arrow up = page up
                gotoPrevPage = 1;
            }

            if(key == KEY_DOWN) {                                   // shift + arrow down = page down
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

void markCurrentRow(void)
{
    commandShort[4] = FDD_CMD_SEARCH_STRING;
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
        (void) Cconws("\33pF3\33q   - download,\r\n");
        (void) Cconws("\33pF8\33q   - setup screen, \33pF10\33q - quit\r\n");
    }
}

void showResults(BYTE showMask)
{
    int i;
    char *pRow;

    if(showMask & SHOWMENU_RESULTS_ALL) {           // if should redraw all results
        for(i=0; i<15; i++) {
            pRow = (char *) (searchContent + (i * 68));

            BYTE selected = (i == search.row);
        
            Goto_pos(0, 3 + i);
        
            if(selected) {                              // for selected row
                (void) Cconws("\33p");
            }   
        
            (void) Cconws(pRow);

            if(selected) {                              // for selected row
                (void) Cconws("\33q");
            }
        }
        
        return;
    }
    
    if(showMask & SHOWMENU_RESULTS_ROW) {           // if should redraw only selected line
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

void showSearchString(void)
{
    Goto_pos(0, 1);
    (void) Cconws("Search:                              ");
    Goto_pos(8, 1);
    (void) Cconws(search.text);
}

BYTE loopForSetup(void)
{
	nextMenuRedrawIsFull = 0;
    showMenu(1);

    while(1) {
        BYTE key;
        BYTE  handled = 0;

		key =  getKey();

		if(key >= 'A' && key <= 'Z') {								// upper case letter? to lower case!
			key += 32;
		}
		
        if(key == KEY_F10 || key == KEY_F9) {                       // should quit or switch mode? 
            return key;
        }
        
        if(key >= '1' && key <= '3') {                              // upload image
            uploadImage(key - '1');
			nextMenuRedrawIsFull = 1;
            handled = 1;
        }
        
        if(key >= '4' && key <= '6') {                              // swap image
            swapImage(key - '4');
            handled = 1;
        }

        if(key >= '7' && key <= '9') {                              // remove image
            removeImage(key - '7');
            handled = 1;
        }
		
		if(key >= 'n' && key <= 'p') {								// create new image
			newImage(key - 'n');
			handled = 1;
		}

		if(key >= 'd' && key <= 'f') {								// download image
			downloadImage(key - 'd');
			handled = 1;
		}
        
        if(handled) {
            showMenu(nextMenuRedrawIsFull);
			nextMenuRedrawIsFull = 0;
        }
    }
}

BYTE getKey(void)
{
	DWORD scancode;
	BYTE key, vkey;

    scancode = Cnecin();					/* get char form keyboard, no echo on screen */

	vkey	= (scancode>>16)	& 0xff;
    key		=  scancode			& 0xff;

    key		= atariKeysToSingleByte(vkey, key);	/* transform BYTE pair into single BYTE */
    
    return key;
}

void showMenu(char fullNotPartial)
{
    if(fullNotPartial) {
		(void) Clear_home();
		(void) Cconws("\33p[CosmosEx floppy config, by Jookie 2014]\33q\r\n");
	    (void) Cconws("\r\n");
		(void) Cconws("Menu:\r\n");
    	(void) Cconws("       upload swap remove new  download\r\n");
    	(void) Cconws("\33pslot 1\33q:   1     4     7     N     D\r\n");
    	(void) Cconws("\33pslot 2\33q:   2     5     8     O     E\r\n");
    	(void) Cconws("\33pslot 3\33q:   3     6     9     P     F\r\n");
    }
	
    getSiloContent();
    showImage(0);
    showImage(1);
    showImage(2);
    
    if(fullNotPartial) {
        Goto_pos(0, 20);
    	(void) Cconws("\r\n\33pF9\33q  to get images from internet.");
    	(void) Cconws("\r\n\33pF10\33q to quit.");
    }
}

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
//    BYTE *content   = &siloContent[(index * 160) + 80];
    
	Goto_pos(0, 12 + index);
	
    (void) Cconws("Image ");
    Cconout(index + '1');
    (void) Cconws(":                        ");

	Goto_pos(11, 12 + index);
    (void) Cconws(filename);
    (void) Cconws("\r\n");

//    (void) Cconws(content);
//    (void) Cconws("\r\n\r\n");
}

void newImage(int index)
{
    if(index < 0 || index > 2) {
        return;
    }

    commandShort[4] = FDD_CMD_NEW_EMPTYIMAGE;
    commandShort[5] = index;

    sectorCount = 1;                            // read 1 sector
    
    BYTE res = Supexec(ce_acsiReadCommand); 
		
	if(res != FDD_OK) {                         // bad? write error
        showComError();
    }
}

void downloadImage(int index)
{
    if(index < 0 || index > 2) {
        return;
    }

    // start the transfer
    commandShort[4] = FDD_CMD_DOWNLOADIMG_START;
    commandShort[5] = index;

    sectorCount = 1;                            // read 1 sector
    
    BYTE res = Supexec(ce_acsiReadCommand); 
		
	if(res == FDD_OK) {                         // good? copy in the results
        strcpy(fileName, (char *) pBfr);        // pBfr should contain original file name
    } else {                                    // bad? show error
        showComError();
        return;
    }
    
    short button;  
  
    // open fileselector and get path
	graf_mouse(M_ON, 0);
    fsel_input(filePath, fileName, &button);    // show file selector
	graf_mouse(M_OFF, 0);
  
    if(button != 1) {                           // if OK was not pressed
        commandShort[4] = FDD_CMD_DOWNLOADIMG_DONE;
        commandShort[5] = index;

        sectorCount = 1;                            // read 1 sector
        
        res = Supexec(ce_acsiReadCommand); 
		
        if(res != FDD_OK) {
            showComError();
        }
        return;
    }
  
	// fileName contains the filename
	// filePath contains the path with search wildcards, e.g.: C:\\*.*
  
    // create full path
    char fullPath[512];

    createFullPath(fullPath, filePath, fileName);       // fullPath = filePath + fileName
    
    short fh = Fcreate(fullPath, 0);               		// open file for writing
    
    if(fh < 0) {
        (void) Clear_home();
        (void) Cconws("Failed to create the file:\r\n");
        (void) Cconws(fullPath);
        (void) Cconws("\r\n");
    
        Cnecin();
        return;
    }

    // do the transfer
    int32_t blockNo, len, ires;
    BYTE failed = 0;
    
    for(blockNo=0; blockNo<64; blockNo++) {                 // try to get all blocks
        commandShort[4] = FDD_CMD_DOWNLOADIMG_GETBLOCK;     // receiving block
        commandShort[5] = (index << 6) | (blockNo & 0x3f);  // for this index and block #
        
        sectorCount = 128;                                  // read 128 sectors (64 kB)
        
        res = Supexec(ce_acsiReadCommand);                  // get the data
		
        if(res != FDD_OK) {                                 // error? write error
            showComError();
            
            failed = 1;
            break;
        }
        
        len = (((WORD) pBfr[0]) << 8) | ((WORD) pBfr[1]);   // retrieve count of data in buffer

        if(len > 0) {                                       // something to write?
            ires = Fwrite(fh, len, pBfr + 2);
            
            if(ires < 0 || ires != len) {                   // failed to write?
                (void) Cconws("Writing to file failed!\r\n");
                Cnecin();
                
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
        showComError();
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
        showComError();
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
        showComError();
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
        showComError();
    }
}

void uploadImage(int index)
{
    if(index < 0 || index > 2) {
        return;
    }

    short button;  
  
    // open fileselector and get path
	graf_mouse(M_ON, 0);
    fsel_input(filePath, fileName, &button);    // show file selector
	graf_mouse(M_OFF, 0);
  
    if(button != 1) {                           // if OK was not pressed
        return;
    }
  
	// fileName contains the filename
	// filePath contains the path with search wildcards, e.g.: C:\\*.*
  
    // create full path
    char fullPath[512];
    createFullPath(fullPath, filePath, fileName);       // fullPath = filePath + fileName  
    
    short fh = Fopen(fullPath, 0);               		// open file for reading
  
    if(fh < 0) {
        (void) Clear_home();
        (void) Cconws("Failed to open the file:\r\n");
        (void) Cconws(fullPath);
        (void) Cconws("\r\n");
    
        Cnecin();
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
		
    if(res == FDD_UPLOADSTART_RES_ONDEVICECOPY) {               // if the device returned this code, it means that it could do the image upload / copy on device, no need to upload it from ST!
        Fclose(fh);
        return;
    }
        
    if(res != FDD_OK) {                                         // bad? write error
        showComError();
                
        Fclose(fh);                                             // close file and quit
        return;
    }
    //---------------
    // upload the image by 64kB blocks
    
    (void) Clear_home();
    (void) Cconws("Uploading file:\r\n");
    (void) Cconws(fullPath);
    (void) Cconws("\r\n");
    
    BYTE good = 1;
    BYTE blockNo = 0;
    
    sectorCount = 128;                                          // write 128 sectors (64 kB)
    
    while(1) {
        Cconout('*');                                           // show progress...

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
            showComError();
               
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
        (void) Clear_home();
        (void) Cconws("Failed to finish upload...\r\n");
        Cnecin();
    }
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

// this function scans the ACSI bus for any active CosmosEx translated drive 
BYTE ce_findId(void)
{
	char bfr[2], res, i;

	bfr[1] = 0;
	deviceID = 0;
	
	(void) Cconws("Looking for CosmosEx: ");

	for(i=0; i<8; i++) {
		bfr[0] = i + '0';
		(void) Cconws(bfr);

		deviceID = i;									// store the tested ACSI ID 
		res = Supexec(ce_identify);  					// try to read the IDENTITY string 
		
		if(res == 1) {                           		// if found the CosmosEx 
			(void) Cconws("\r\nCosmosEx found on ACSI ID: ");
			bfr[0] = i + '0';
			(void) Cconws(bfr);

			return 1;
		}
	}

	// if not found 
    (void) Cconws("\r\nCosmosEx not found on ACSI bus, not installing driver.");
	return 0;
}

// send an IDENTIFY command to specified ACSI ID and check if the result is as expected 
BYTE ce_identify(void)
{
	WORD res;
  
	commandShort[0] = (deviceID << 5); 											// cmd[0] = ACSI_id + TEST UNIT READY (0)	
	commandShort[4] = FDD_CMD_IDENTIFY;
  
	memset(pBfr, 0, 512);              											// clear the buffer 

	res = acsi_cmd(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pBfr, 1);			// issue the command and check the result 

	if(res != OK) {                        										// if failed, return FALSE 
		return 0;
	}

	if(strncmp((char *) pBfr, "CosmosEx floppy setup", 21) != 0) {				// the identity string doesn't match? 
		return 0;
	}
	
	return 1;                             										// success 
}

// make single ACSI read command by the params set in the commandShort buffer
BYTE ce_acsiReadCommand(void)
{
	WORD res;
  
	commandShort[0] = (deviceID << 5); 											// cmd[0] = ACSI_id + TEST UNIT READY (0)	
  
	memset(pBfr, 0, 512);              											// clear the buffer 

	res = acsi_cmd(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pBfr, sectorCount);   // issue the command and check the result 

	return res;                            										// success 
}

BYTE ce_acsiWriteBlockCommand(void)
{
	WORD res;
  
	commandShort[0] = (deviceID << 5); 											// cmd[0] = ACSI_id + TEST UNIT READY (0)	
  
	res = acsi_cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, p64kBlock, sectorCount);	// issue the command and check the result 

    return res;                                                                 // just return the code
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
/*
    (void) Clear_home();
    (void) Cconws("Error in CosmosEx communication!\r\n");
    Cnecin();
*/    
}

void createFullPath(char *fullPath, char *filePath, char *fileName)
{
    strcpy(fullPath, filePath);
	
	removeLastPartUntilBackslash(fullPath);				// remove the search wildcards from the end

	if(strlen(fullPath) > 0) {							
		if(fullPath[ strlen(fullPath) - 1] != '\\') {	// if the string doesn't end with backslash, add it
			strcat(fullPath, "\\");
		}
	}
	
    strcat(fullPath, fileName);							// add the filename
}

BYTE atariKeysToSingleByte(BYTE vkey, BYTE key)
{
	WORD vkeyKey;

	if(key >= 32 && key < 127) {		/* printable ASCII key? just return it */
		return key;
	}
	
	if(key == 0) {						/* will this be some non-ASCII key? convert it */
		switch(vkey) {
			case 0x48: return KEY_UP;
			case 0x50: return KEY_DOWN;
			case 0x4b: return KEY_LEFT;
			case 0x4d: return KEY_RIGHT;
			case 0x52: return KEY_INSERT;
			case 0x47: return KEY_HOME;
			case 0x62: return KEY_HELP;
			case 0x61: return KEY_UNDO;
			case 0x3b: return KEY_F1;
			case 0x3c: return KEY_F2;
			case 0x3d: return KEY_F3;
			case 0x3e: return KEY_F4;
			case 0x3f: return KEY_F5;
			case 0x40: return KEY_F6;
			case 0x41: return KEY_F7;
			case 0x42: return KEY_F8;
			case 0x43: return KEY_F9;
			case 0x44: return KEY_F10;
			default: return 0;			/* unknown key */
		}
	}
	
	vkeyKey = (((WORD) vkey) << 8) | ((WORD) key);		/* create a WORD with vkey and key together */
	
	switch(vkeyKey) {					/* some other no-ASCII key, but check with vkey too */
		case 0x011b: return KEY_ESC;
		case 0x537f: return KEY_DELETE;
		case 0x0e08: return KEY_BACKSP;
		case 0x0f09: return KEY_TAB;
		case 0x1c0d: return KEY_ENTER;
		case 0x720d: return KEY_ENTER;
	}

	return 0;							/* unknown key */
}

void setSelectedRow(int row)
{
    search.prevRow  = search.row;           // store current line as previous one
    search.row      = row;                  // store new line as the current one
}
