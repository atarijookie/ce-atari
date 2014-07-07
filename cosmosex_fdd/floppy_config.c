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

// ------------------------------------------------------------------ 
BYTE loopForSetup(void)
{
	BYTE nextMenuRedrawIsFull = 0;
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
    if((index < 0 || index > 2) && index != 10) {       // if it's not normal floppy index and not internet download index
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
    
    //--------------------------------
    if(index == 10) {                               // for internet download just use predefined destination dir
    
    
    
    
    } else {                                        // for ordinary floppy slots open file selector
        // open fileselector and get path
        graf_mouse(M_ON, 0);
        short button;  
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

