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

       
// ------------------------------------------------------------------ 
BYTE ce_findId(void);
BYTE ce_identify(void);

BYTE dmaBuffer[DMA_BUFFER_SIZE + 2];
BYTE *pDmaBuffer;

BYTE deviceID;

BYTE commandShort[CMD_LENGTH_SHORT]	= {	0, 'C', 'E', HOSTMOD_FDD_SETUP, 0, 0};

BYTE siloContent[512];

void showMenu(char fullNotPartial);
void showImage(int index);
void getSiloContent(void);

void uploadImage(int index);
void swapImage(int index);
void removeImage(int index);

BYTE getLowestDrive(void);
void removeLastPartUntilBackslash(char *str);

BYTE ce_acsiReadCommand(void);
BYTE ce_acsiWriteBlockCommand(void);

void newImage(int index);
void downloadImage(int index);

char filePath[256], fileName[256];

BYTE *p64kBlock;
BYTE sectorCount;

#define GOTO_POS        "\33Y"
#define Goto_pos(x,y)   ((void) Cconws(GOTO_POS),  (void) Cconout(' ' + y), (void) Cconout(' ' + x))

// ------------------------------------------------------------------ 
int main( int argc, char* argv[] )
{
	BYTE found;
  
    appl_init();										            // init AES 
    
    // init fileselector path
    strcpy(filePath, "C:\\*.*");                            
    memset(fileName, 0, 256);          
    
    BYTE drive = getLowestDrive();                                  // get the lowest HDD letter and use it in the file selector
    filePath[0] = drive;
    
	// write some header out
	(void) Clear_home();
	(void) Cconws("\33p[ CosmosEx floppy setup ]\r\n[    by Jookie 2014     ]\33q\r\n\r\n");

	// create buffer pointer to even address 
	pDmaBuffer = &dmaBuffer[2];
	pDmaBuffer = (BYTE *) (((DWORD) pDmaBuffer) & 0xfffffffe);		// remove odd bit if the address was odd 

	// search for CosmosEx on ACSI bus
	found = ce_findId();

	if(!found) {								                    // not found? quit
		sleep(3);
		return 0;
	}
 
	// now set up the acsi command bytes so we don't have to deal with this one anymore 
	commandShort[0] = (deviceID << 5); 					            // cmd[0] = ACSI_id + TEST UNIT READY (0)	
	
    showMenu(1);

    while(1) {
    	BYTE key;
        DWORD scancode;
        BYTE  handled = 0;

   		scancode = Cnecin();					                    // get char form keyboard, no echo on screen

		key		=  scancode & 0xff;

		if(key >= 'A' && key <= 'Z') {								// upper case letter? to lower case!
			key += 32;
		}
		
        if(key == 'q') {                              				// should quit?
            break;
        }
        
        if(key >= '1' && key <= '3') {                              // upload image
            uploadImage(key - '1');
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
            showMenu(0);
        }
    }
    

	return 0;		
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
    	(void) Cconws("\r\n\33pPress Q to quit.\33q\r\n\r\n\r\n");
    }
	
    getSiloContent();
    showImage(0);
    showImage(1);
    showImage(2);
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

    BYTE res = Supexec(ce_acsiReadCommand); 
		
	if(res != 1) {                              // bad? write error
        (void) Clear_home();
        (void) Cconws("Error in CosmosEx communication!\r\n");
        Cnecin();
    }
}

void downloadImage(int index)
{
    if(index < 0 || index > 2) {
        return;
    }
	
	// TODO: all the code for download
}

void getSiloContent(void)
{
    commandShort[4] = FDD_CMD_GETSILOCONTENT;

    BYTE res = Supexec(ce_acsiReadCommand); 
		
	if(res == 1) {                              // good? copy in the results
        memcpy(siloContent, pDmaBuffer, 512);
    } else {                                    // bad? show error
        (void) Clear_home();
        (void) Cconws("Error in CosmosEx communication!\r\n");
        Cnecin();
    }
}

void swapImage(int index)
{
    if(index < 0 || index > 2) {
        return;
    }

    commandShort[4] = FDD_CMD_SWAPSLOTS;
    commandShort[5] = index;

    BYTE res = Supexec(ce_acsiReadCommand); 
		
	if(res != 1) {                              // bad? write error
        (void) Clear_home();
        (void) Cconws("Error in CosmosEx communication!\r\n");
        Cnecin();
    }
}

void removeImage(int index)
{
    if(index < 0 || index > 2) {
        return;
    }

    commandShort[4] = FDD_CMD_REMOVESLOT;
    commandShort[5] = index;

    BYTE res = Supexec(ce_acsiReadCommand); 
		
	if(res != 1) {                              // bad? write error
        (void) Clear_home();
        (void) Cconws("Error in CosmosEx communication!\r\n");
        Cnecin();
    }
}

void uploadImage(int index)
{
    if(index < 0 || index > 2) {
        return;
    }

    short button;  
  
    // open fileselector and get path
    fsel_input(filePath, fileName, &button);    // show file selector
  
    if(button != 1) {                           // if OK was not pressed
        return;
    }
  
	// fileName contains the filename
	// filePath contains the path with search wildcards, e.g.: C:\\*.*
  
    // create full path
    char fullPath[512];
    strcpy(fullPath, filePath);
	
	removeLastPartUntilBackslash(fullPath);				// remove the search wildcards from the end

	if(strlen(fullPath) > 0) {							
		if(fullPath[ strlen(fullPath) - 1] != '\\') {	// if the string doesn't end with backslash, add it
			strcat(fullPath, "\\");
		}
	}
	
    strcat(fullPath, fileName);							// add the filename
  
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
    // these buffers will be handy in a while...

    #define SIZE64K     (64*1024)
    
    BYTE imgBlock[SIZE64K + 4];
    BYTE *pBfr      = (BYTE *) (((DWORD) (&imgBlock[4])) & 0xfffffffe);     // create even pointer
    BYTE *pBfrCnt   = pBfr - 2;                                             // pointer to where the 
    BYTE res;
    
    //---------------
    // tell the device the path and filename of the source image
    
    commandShort[4] = FDD_CMD_UPLOADIMGBLOCK_START;             // starting image upload
    commandShort[5] = index;                                    // for this index
    
    p64kBlock       = pBfr;                                     // use this buffer for writing
    strcpy((char *) pBfr, fullPath);                            // and copy in the full path
    
    sectorCount     = 1;                                        // write just one sector
    
    res = Supexec(ce_acsiWriteBlockCommand); 
		
    if(res == FDD_UPLOADSTART_RES_ONDEVICECOPY) {               // if the device returned this code, it means that it could do the image upload / copy on device, no need to upload it from ST!
        Fclose(fh);
        return;
    }
        
    if(res != 0) {                                              // bad? write error
        (void) Clear_home();
        (void) Cconws("Error in CosmosEx communication!\r\n");
        Cnecin();
                
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

        if(res != 0) {                                          // error? write error
            (void) Clear_home();
            (void) Cconws("Error in CosmosEx communication!\r\n");
            Cnecin();
               
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

    res = Supexec(ce_acsiReadCommand);     

    if(res != 1) {                              // bad? write error
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
  
	memset(pDmaBuffer, 0, 512);              									// clear the buffer 

	res = acsi_cmd(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);	// issue the command and check the result 

	if(res != OK) {                        										// if failed, return FALSE 
		return 0;
	}

	if(strncmp((char *) pDmaBuffer, "CosmosEx floppy setup", 21) != 0) {		// the identity string doesn't match? 
		return 0;
	}
	
	return 1;                             										// success 
}

// make single ACSI read command by the params set in the commandShort buffer
BYTE ce_acsiReadCommand(void)
{
	WORD res;
  
	commandShort[0] = (deviceID << 5); 											// cmd[0] = ACSI_id + TEST UNIT READY (0)	
  
	memset(pDmaBuffer, 0, 512);              									// clear the buffer 

	res = acsi_cmd(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);	// issue the command and check the result 

	if(res != OK) {                        										// if failed, return FALSE 
		return 0;
	}

	return 1;                             										// success 
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
