#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <unistd.h>

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

// ------------------------------------------------------------------ 

BYTE uploadImage(int index, char *path)
{
    if(index < 0 || index > 2) {
        return 0;
    }

    short fh = Fopen(path, 0);               		// open file for reading
  
    if(fh < 0) {
        (void) Cconws("Failed to open the file:\r\n");
        (void) Cconws(path);
        (void) Cconws("\r\n");
        return 0;
    }

    //---------------
    // tell the device the path and filename of the source image
    
    commandShort[4] = FDD_CMD_UPLOADIMGBLOCK_START;             // starting image upload
    commandShort[5] = index;                                    // for this index
    
    p64kBlock       = pBfr;                                     // use this buffer for writing
    strcpy((char *) pBfr, path);                                // and copy in the full path
    
    sectorCount     = 1;                                        // write just one sector
    
    BYTE res;
    res = Supexec(ce_acsiWriteBlockCommand); 
		
    if(res == FDD_RES_ONDEVICECOPY) {                           // if the device returned this code, it means that it could do the image upload / copy on device, no need to upload it from ST!
        Fclose(fh);
        return 1;
    }
        
    if(res != FDD_OK) {                                         // bad? write error
        showComError();
                
        Fclose(fh);                                             // close file and quit
        return 0;
    }
    //---------------
    // upload the image by 64kB blocks
    
    (void) Cconws("Uploading file:\r\n");
    (void) Cconws(path);
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

    (void) Cconws("\r\n");
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
        (void) Cconws("Failed to finish upload...\r\n");
        return 0;
    } 
    
    return 1;
}

