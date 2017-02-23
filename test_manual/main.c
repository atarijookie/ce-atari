//--------------------------------------------------
#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <support.h>

#include <stdint.h>
#include <stdio.h>

#include "stdlib.h"
#include "acsi.h"
#include "scsi.h"
#include "global.h"
#include "translated.h"
#include "gemdos.h"

#include "hdd_if.h"
#include "mutex.h"

#define SCSI_C_READ10       0x28

//--------------------------------------------------

#define E_OK                0           // 00        = No error
#define E_CRC               0xfc        // -4 = 0xfc = CRC error

BYTE deviceID;                          // bus ID from 0 to 7
volatile mutex mtx;

void showHexByte (BYTE val);
void showHexWord (WORD val);
void showHexDword(DWORD val);

void scsi_reset(void);

void cs_inquiry(BYTE id, BYTE verbose);
void CEread(BYTE verbose);
void findDevice(void);

void CEwrite(void);
int  writeHansTest(int byteCount, WORD xorVal);
void showInt(int value, int length);
void largeRead(void);
void SDread(void);

BYTE showLogs = 1;
void showMenu(void);

void logMsg(char *logMsg);

#define RW_TEST_SIZE    MAXSECTORS

BYTE  *pBufferOrig;
BYTE  *pBuffer;
DWORD largeMemSizeInBytes;
DWORD largeMemSizeInSectors;

void hdIfCmdAsUser(BYTE readNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount);

DWORD largestMemBlock;

//-------------------------------------------------- 
#define MACHINE_ST      0
#define MACHINE_TT      2
#define MACHINE_FALCON  3

BYTE machineType = MACHINE_ST;
//-------------------------------------------------- 
void getMachineType(void)
{
    DWORD *cookieJarAddr    = (DWORD *) 0x05A0;
    DWORD *cookieJar        = (DWORD *) *cookieJarAddr;     // get address of cookie jar

    // by default - it's and ST
    machineType = MACHINE_ST;

    if(cookieJar == 0) {                        // no cookie jar? it's an old ST
        return;
    }

    DWORD cookieKey, cookieValue;

    while(1) {                                  // go through the list of cookies
        cookieKey   = *cookieJar++;
        cookieValue = *cookieJar++;

        if(cookieKey == 0) {                    // end of cookie list? then cookie not found, it's an ST
            break;
        }

        if(cookieKey == 0x5f4d4348) {           // is it _MCH key?
            WORD machine = cookieValue >> 16;

            switch(machine) {                   // depending on machine, either it's TT or FALCON
                case 2: machineType = MACHINE_TT;       return;
                case 3: machineType = MACHINE_FALCON;   return;
            }

            break;                              // or it's ST
        }
    }

    // it's an ST
}
//-------------------------------------------------- 
void getLargestMemBlock(void)
{
    _MPB mpb;

    Getmpb(&mpb);                                   // get Memory parameter block
    _MD *mpFree = mpb.mp_free;                      // get pointer to list of free blocks

    largestMemBlock = 0;

    while(mpFree != NULL) {                         // while there is a valid free block
        DWORD blockStart    =  (DWORD) mpFree->md_start;    // get starting address
        BYTE  isFastRam     =  (blockStart >= 0x1000000) ? TRUE  : FALSE;
    
        if(isFastRam) {                                     // if it's FAST RAM, ignore this block
            continue;
        }
    
        if(largestMemBlock < mpFree->md_length) {   // current block if larger than the largest found yet?
            largestMemBlock = mpFree->md_length;
        }

        mpFree = mpFree->md_next;                   // move to next block
    }
}

//--------------------------------------------------
int main(void)
{
    DWORD scancode;
    BYTE key;
    DWORD toEven;

    Clear_home();
    //                |                                        |             
    (void) Cconws("\33p>>>>>> Manual HDD test tool <<<<<<\33q\r\n\r\n");

    #define SIZE_13MB       (14 * 1024 * 1024)
    #define SIZE_4MB        ( 4 * 1024 * 1024)
    #define SIZE_MAXSECTORS (MAXSECTORS * 512)
    
    // ---------------------- 
    // get what machine we're running on
    Supexec(getMachineType);

    //-----------------------
    // find the largest free block of memory
    Supexec(getLargestMemBlock);

    //            |                    |
    (void) Cconws("Largest mem block   : ");
    showHexDword(largestMemBlock);
    (void) Cconws("\r\n");
    
    if(largestMemBlock < SIZE_MAXSECTORS) {
        //            |                                        |             
        (void) Cconws("Not enough free memory, expecting\r\n");
        (void) Cconws("at least 128 kB free!\r\n");
        (void) Cconws("Press any key to terminate.\r\n");
        
        Cnecin();
        return 0;
    }
    
    if(machineType == MACHINE_ST) {                     // for ST - just stick to max sectors size
        largestMemBlock = SIZE_MAXSECTORS;
    }
    
    if(largestMemBlock >= SIZE_13MB) {                  // something is bigger than 13 MB? Limit it to 13 MB, which is near max ST RAM size
        largestMemBlock = SIZE_13MB;
    }

    //            |                    |
    (void) Cconws("Will try to alloc   : ");
    showHexDword(largestMemBlock);
    (void) Cconws("\r\n");

    if(largestMemBlock < SIZE_4MB) {                    // less than 4 MB? use normal Malloc()
        (void) Cconws("Doing Malloc()...\r\n");
        pBufferOrig = (BYTE *) Malloc(largestMemBlock);
    } else {                                            // more than 4 MB? use Mxalloc(), so we can force it into ST RAM
        (void) Cconws("Doing Mxalloc()...\r\n");
        pBufferOrig = (BYTE *) Mxalloc(largestMemBlock, 1);
    }

    if(pBufferOrig == NULL) {
        //            |                                        |             
        (void) Cconws("Failed to allocate memory!\r\n");
        (void) Cconws("Press any key to terminate.\r\n");
        
        Cnecin();
        return 0;
    }
    
    largeMemSizeInBytes     = largestMemBlock;          // store how much we got - in bytes
    largeMemSizeInSectors   = largeMemSizeInBytes >> 9; // how much we got       - in sectors

    // ---------------------- 
    // create buffer pointer to even address 
    toEven = (DWORD) &pBufferOrig[0];

    if(toEven & 0x0001) {       // not even number? 
        toEven++;
        largeMemSizeInBytes--;
    }

    pBuffer = (BYTE *) toEven; 

     //           |                    |
    (void) Cconws("Large mem size (B)  : ");
    showHexDword(largeMemSizeInBytes);
    //                |                    |
    (void) Cconws("\r\nLarge mem size (s)  : ");
    showHexDword(largeMemSizeInSectors);
    (void) Cconws("\r\n");

    // ---------------------- 
    // search for device on the ACSI / SCSI bus 
    deviceID = 0;
    
    //initialize lock
    mutex_unlock(&mtx);
    
    if(machineType == MACHINE_ST) {             // if it's ST, use ACSI
        key = 'a';
    } else if(machineType == MACHINE_FALCON) {  // if it's Falcon, use SCSI
        key = 'f';
    } else {                                    // if it's TT, let user choose
        //            |                    |
        (void) Cconws("Choose HDD interface:\r\n");
        
        while(1) {
            (void) Cconws("'A' - ACSI \r\n");
            (void) Cconws("'T' - TT SCSI \r\n");

            key = Cnecin();
            if(key >= 'A' && key <= 'Z') {
                key += 32;
            }
            
            if(key == 't' || key == 'a') {      // good key press? go on...
                break;
            }
        }
    }

     //           |                    |
    (void) Cconws("HDD Interface       : ");
    
    switch(key) {
        case 'a':   
            (void) Cconws("\33pACSI\33q");
            hdd_if_select(IF_ACSI);
            deviceID = 0;           // ACSI ID 0
            break;

        case 't':
            (void) Cconws("\33pTT SCSI\33q");
            hdd_if_select(IF_SCSI_TT);
            deviceID = 0;           // SCSI ID 0
            break;

        case 'f':
            (void) Cconws("\33pFalcon SCSI\33q");
            hdd_if_select(IF_SCSI_FALCON);
            deviceID = 1;           // SCSI ID 1
            
            break;
    } 
    (void) Cconws("\r\n");

    showLogs = 0;                   // turn off logs - there will be errors on findDevice when device doesn't exist 
    Supexec(findDevice);
    showLogs = 1;                   // turn on logs
            
    showMenu();

    //-----------------
    // main menu loop
    while(1) {
        scancode    = Bconin(DEV_CONSOLE); 		                    // get char form keyboard, no echo on screen 
        key         =  scancode & 0xff;

        if(key == 'q') {
            (void) Cconws("Terminating...\r\n");
            sleep(1);
            break;
        }

        if(key == 'x') {
            (void) Cconws("SCSI reset...");
            scsi_reset();
            (void) Cconws("done\r\n");
            continue;
        }

        if(key == 'i') {            // INQUIRY command
            cs_inquiry(deviceID, 1);
            continue;
        }

        if(key == 'I') {
            int i;
            DWORD start, end, diff;

            start = getTicks();
            for(i=0; i<10; i++) {
                cs_inquiry(deviceID, 0);
            }
            end = getTicks();
            diff = end - start;
            int timeMs  = (diff * 1000) / 200;

            (void) Cconws("\n\rINQUIRY x 10 took: ");
            showInt(timeMs, -1);
            (void) Cconws(" ms\n\r");

            continue;
        }

        if(key == 's') {            // read 
            SDread();
            continue;
        }

        if(key == 'r') {            // read 
            CEread(1);
            continue;
        }

        if(key == 'R') {
            int i;
            DWORD start, end, diff;

            start = getTicks();
            for(i=0; i<10; i++) {
                CEread(0);
            }
            end = getTicks();
            diff = end - start;

            diff    = end - start;

            int timeMs  = (diff * 1000) / 200;
            int kbps    = ((10 * MAXSECTORS) * 500) / timeMs;

            (void) Cconws("\n\rREAD x 10 time : ");
            showInt(timeMs, -1);
            (void) Cconws(" ms\n\r");

            (void) Cconws("\n\rREAD x 10 speed: ");
            showInt(kbps, -1);
            (void) Cconws(" kB/s\n\r");

            continue;
        }

        if(key == 'w') {            // write
            CEwrite();
            continue;
        }

        if(key == 'l' || key == 'L') {
            largeRead();
        }

        if(key == 'f') {
            showLogs = 0;           // turn off logs - there will be errors on findDevice when device doesn't exist 
            Supexec(findDevice);
            showLogs = 1;           // turn on logs
            continue;
        }

        if(key == 'c') {            // clear screen and show menu again
            Clear_home();
            showMenu();
            continue;
        }
    }

    Mfree(pBufferOrig);             // release the memory
    return 0;
}

void showMenu(void)
{
    (void) Cconws("Device ID: ");
    showHexByte(deviceID);
    (void) Cconws("\r\n");
    
    (void) Cconws("x - SCSI reset\r\n");
    (void) Cconws("i -  1 x INQUIRY\r\n");
    (void) Cconws("I - 10 x INQUIRY\r\n");
    (void) Cconws("s -  1 x SD card READ\r\n");
    (void) Cconws("r -  1 x READ\r\n");
    (void) Cconws("R - 10 x READ\r\n");
    (void) Cconws("w - CE WRITE test\r\n");
    (void) Cconws("L - large read\r\n");
    (void) Cconws("f - Find device on SCSI\r\n");
    (void) Cconws("c - clear screen\r\n");
    (void) Cconws("Q - quit\r\n\r\n");
}

BYTE commandLong[CMD_LENGTH_LONG] = {0x1f,	0, 'C', 'E', HOSTMOD_TRANSLATED_DISK, 0, 0, 0, 0, 0, 0, 0, 0}; 
int readHansTest(int byteCount, WORD xorVal, BYTE verbose);

void CEread(BYTE verbose)
{
  	commandLong[0] = (deviceID << 5) | 0x1f;			// cmd[0] = ACSI_id + ICD command marker (0x1f)	
	commandLong[1] = 0xA0;								// cmd[1] = command length group (5 << 5) + TEST UNIT READY (0)  	

    WORD xorVal=0xC0DE;
    
    int res = readHansTest(RW_TEST_SIZE * 512, xorVal, verbose);
    
    switch( res )
    {
        case -1:    (void) Cconws("TIMEOUT\r\n");   break;
        case -2:    (void) Cconws("CRC FAIL\r\n");  break;
        case 0:     if(verbose) { (void) Cconws("GOOD\r\n"); }  break;
        default:    (void) Cconws("ERROR\r\n");     break;
    }
}

void largeRead(void)
{
    DWORD mbCount       = (largeMemSizeInSectors      >> 11);   // sectors to mega bytes
    DWORD timeoutSecs   = mbCount * 3;                          // mega bytes to seconds
    
    (void) Cconws("READ(10) - dev: ");
    showInt(deviceID, 1);
    (void) Cconws(", size: ");
    showInt(mbCount, 2);
    (void) Cconws(" MB, timeout: ");
    showInt(timeoutSecs, 2);
    (void) Cconws(" s\r\n");
    
    memset(commandLong, 0, sizeof(commandLong));

    commandLong[0] = (deviceID << 5) | 0x1f;
    commandLong[1] = SCSI_C_READ10;
    
    commandLong[8] = (BYTE) (largeMemSizeInSectors >> 8);
    commandLong[9] = (BYTE) (largeMemSizeInSectors     );
    
    hdIfCmdAsUser(1, commandLong, 11, pBuffer, largeMemSizeInSectors);

    (void) Cconws("Command success: ");
    showHexByte(hdIf.success);
    (void) Cconws("\r\n");
    
    (void) Cconws("SCSI result    : ");
    showHexByte(hdIf.statusByte);
    (void) Cconws("\r\n");
}

int readHansTest(int byteCount, WORD xorVal, BYTE verbose)
{
	commandLong[4+1] = TEST_READ;

    //size to read
	commandLong[5+1] = (byteCount >> 16) & 0xFF;
	commandLong[6+1] = (byteCount >>  8) & 0xFF;
	commandLong[7+1] = (byteCount      ) & 0xFF;

    //Word to XOR with data on CE side
	commandLong[8+1] = (xorVal >> 8) & 0xFF;
	commandLong[9+1] = (xorVal     ) & 0xFF;

    if(verbose) {
        (void) Cconws("CE READ: ");
    }
    
    hdIfCmdAsUser(ACSI_READ, commandLong, CMD_LENGTH_LONG, pBuffer, byteCount >> 9);      // issue the command and check the result
    
    if(!hdIf.success) {                                                                  // ACSI ERROR?
        return -1;
    }
    
    if(!verbose) {
        return 0;
    }
    
    int i;
    WORD counter = 0;
    WORD data = 0;
    for(i=0; i<byteCount; i += 2) {
        data = counter ^ xorVal;       // create word
        if( !(pBuffer[i]==(data>>8) && pBuffer[i+1]==(data&0xFF)) ){
          return -2;
        }  
        counter++;
    }

    if(byteCount & 1) {                                 // odd number of bytes? add last byte
        BYTE lastByte = (counter ^ xorVal) >> 8;
        if( pBuffer[byteCount-1]!=lastByte ){
          return -2;
        }  
    }
    
	return 0;
}

#define SCSI_C_READ6            0x08
#define SCSI_C_REQUEST_SENSE    0x03

void SDread(void)
{
    BYTE cmd[6];
    
    memset(cmd, 0, 6);
    cmd[0] = (deviceID << 5) | SCSI_C_READ6;
    cmd[4] = 1;
    
    (void) Cconws("SD READ...\r\n");
    
    // issue the inquiry command and check the result 
    hdIfCmdAsUser(1, cmd, 6, pBuffer, 1);

    (void) Cconws("  success: ");
    showHexByte(hdIf.success);
    (void) Cconws("\r\n");

    (void) Cconws("  status : ");
    showHexByte(hdIf.statusByte);
    (void) Cconws("\r\n");

    if(hdIf.success && hdIf.statusByte != 0) {
        (void) Cconws("REQUEST SENSE...\r\n");

        cmd[0] = (deviceID << 5) | SCSI_C_REQUEST_SENSE;
        cmd[4] = 16;                                    // how many bytes should be sent

        hdIfCmdAsUser(1, cmd, 6, pBuffer, 1);
        
        if(!hdIf.success || hdIf.statusByte != 0) {
            (void) Cconws("  ...fail...");
            return;            
        }

        (void) Cconws("  SENSE KEY : ");
        showHexByte(pBuffer[2]);
        (void) Cconws("\r\n");

        (void) Cconws("  SENSE CODE: ");
        showHexByte(pBuffer[12]);
        (void) Cconws("\r\n");

        (void) Cconws("  ASCQ      : ");
        showHexByte(pBuffer[13]);
        (void) Cconws("\r\n");
    }
}

void cs_inquiry(BYTE id, BYTE verbose)
{
	int i;
    BYTE cmd[6];
    
    memset(cmd, 0, 6);
    cmd[0] = (id << 5) | (SCSI_CMD_INQUIRY & 0x1f);
    cmd[4] = 32;                                // count of bytes we want from inquiry command to be returned

    if(verbose) {
        (void) Cconws("SCSI Inquiry: ");
    }    
    
    // issue the inquiry command and check the result 
    hdIfCmdAsUser(1, cmd, 6, pBuffer, 1);

    if(!hdIf.success || verbose) {  // if fail or verbose, show result
        (void) Cconws("SCSI result : ");
        showHexByte(hdIf.statusByte);
        (void) Cconws("\r\n");
    }
    
    if(!hdIf.success) {             // if failed, don't dump anything, it would be just garbage
        return;
    }
    
    if(!verbose) {                  // not verbose? quit, because the rest is just being verbose...
        return;
    }    
    //----------------------------
    // hex dump of data
    (void) Cconws("INQUIRY HEXA: ");

    for(i=0; i<32; i++) {
        showHexByte(pBuffer[i]);
        (void) Cconout(' ');
    }
    (void) Cconws("\r\n");
    
    //----------------------------
    // char dump of data
    (void) Cconws("INQUIRY CHAR: ");
    for(i=0; i<32; i++) {
        if(pBuffer[i] >= 32) {
            Cconout(pBuffer[i]);
        } else {
            Cconout(' ');
        }

        Cconout(' ');
        Cconout(' ');
    }
    
    (void) Cconws("\r\n");
}
//--------------------------------------------------
void showHexByte(BYTE val)
{
    int hi, lo;
    char tmp[3];
    char table[16] = {"0123456789ABCDEF"};
    
    hi = (val >> 4) & 0x0f;;
    lo = (val     ) & 0x0f;

    tmp[0] = table[hi];
    tmp[1] = table[lo];
    tmp[2] = 0;
    
    (void) Cconws(tmp);
}

void showHexWord(WORD val)
{
    BYTE a,b;
    a = val >>  8;
    b = val;
    
    showHexByte(a);
    showHexByte(b);
}

void showHexDword(DWORD val)
{
    BYTE a,b,c,d;
    a = val >> 24;
    b = val >> 16;
    c = val >>  8;
    d = val;
    
    showHexByte(a);
    showHexByte(b);
    showHexByte(c);
    showHexByte(d);
}

BYTE ce_identify(BYTE bus_id)
{
    BYTE cmd[] = {0, 'C', 'E', HOSTMOD_TRANSLATED_DISK, TRAN_CMD_IDENTIFY, 0};

    cmd[0] = (bus_id << 5);                   // cmd[0] = ACSI_id + TEST UNIT READY (0)	
    memset(pBuffer, 0, 512);                  // clear the buffer 

    hdIfCmdAsUser(1, cmd, 6, pBuffer, 1);       // issue the identify command and check the result 

    if(!hdIf.success) {                       // if failed, return FALSE 
        return 0;
    }

    if(strncmp((char *) pBuffer, "CosmosEx translated disk", 24) != 0) {		// the identity string doesn't match? 
        return 0;
    }

    return 1;                             // success 
}

//--------------------------------------------
BYTE cs_inquiry2(BYTE id)
{
    BYTE cmd[CMD_LENGTH_SHORT];
    
    memset(cmd, 0, 6);
    cmd[0] = (id << 5) | (SCSI_CMD_INQUIRY & 0x1f);
    cmd[4] = 32;                                                    // count of bytes we want from inquiry command to be returned
    
    hdIfCmdAsUser(ACSI_READ, cmd, CMD_LENGTH_SHORT, pBuffer, 1);    // issue the inquiry command and check the result 
    
    if(!hdIf.success || hdIf.statusByte != 0) {                     // if failed, return FALSE 
        return FALSE;
    }

    return TRUE;
}

//--------------------------------------------
void findDevice(void)
{
    BYTE i;
    BYTE res;

    //            |                    |
    (void) Cconws("Looking for CosmosEx: ");

    deviceID = 0;
    BYTE good;

    for(i=0; i<8; i++) {
        res     = cs_inquiry2(i);                                   // try to read the IDENTITY string
        good    = FALSE;

        if(res) {
            if(memcmp(pBuffer + 16, "CosmosEx", 8) == 0) {          // inquiry string contains 'CosmosEx'
                if(memcmp(pBuffer + 27, "SD", 2) == 0) {            // it's CosmosEx SD card
                    good = TRUE;
                } else {                                            // it's CosmosEx, but not SD card
                    good = FALSE;
                }
            } else if(memcmp(pBuffer + 16, "CosmoSolo", 9) == 0) {  // it's CosmoSolo, that's SD card
                good = TRUE;
            }
        }

        if(good) {          // good
            (void) Cconws("\33p");
            Cconout('0' + i);
            (void) Cconws("\33q");

            deviceID = i;
        } else {            // bad
            Cconout('0' + i);
        }
    }
    
    (void) Cconws("\n\r");
}

//--------------------------------------------
void CEwrite(void)
{
    commandLong[0] = (deviceID << 5) | 0x1f;			// cmd[0] = ACSI_id + ICD command marker (0x1f)	
    commandLong[1] = 0xA0;								// cmd[1] = command length group (5 << 5) + TEST UNIT READY (0)  	

    WORD xorVal=0xC0DE;
    
    int res = writeHansTest(RW_TEST_SIZE * 512, xorVal);
    
    switch( res )
    {
        case -1:    (void) Cconws("TIMEOUT\r\n");   break;
        case -2:    (void) Cconws("CRC FAIL\r\n");  break;
        case 0:     (void) Cconws("GOOD\r\n");      break;
        default:    (void) Cconws("ERROR\r\n");     break;
    }
}
        
int writeHansTest(int byteCount, WORD xorVal)
{
    commandLong[4+1] = TEST_WRITE;

    //size to read
    commandLong[5+1] = (byteCount >> 16) & 0xFF;
    commandLong[6+1] = (byteCount >>  8) & 0xFF;
    commandLong[7+1] = (byteCount      ) & 0xFF;

    //Word to XOR with data on CE side
    commandLong[8+1] = (xorVal >> 8) & 0xFF;
    commandLong[9+1] = (xorVal     ) & 0xFF;

    int i;
    WORD counter = 0;
    WORD data = 0;
    for(i=0; i<byteCount; i += 2) {
        data = counter ^ xorVal;       // create word
        pBuffer[i] = (data>>8);
        pBuffer[i+1] = (data&0xFF);
        counter++;
    }

    if(byteCount & 1) {                                 // odd number of bytes? add last byte
        BYTE lastByte = (counter ^ xorVal) >> 8;
        pBuffer[byteCount-1]=lastByte;
    }

    (void) Cconws("CE WRITE: ");
    hdIfCmdAsUser(ACSI_WRITE, commandLong, CMD_LENGTH_LONG, pBuffer, byteCount >> 9);     // issue the command and check the result
    
    if(hdIf.statusByte == E_CRC) {                                                            
        return -2;
    }
    if(!hdIf.success) {                                                             
        return -1;
    }
    
    return 0;
}

void logMsg(char *logMsg)
{
    if(showLogs) {
        (void) Cconws(logMsg);
    }
}

void logMsgProgress(DWORD current, DWORD total)
{
    if(!showLogs) {
        return;
    }

    (void) Cconws("Progress: ");
    showHexDword(current);
    (void) Cconws(" out of ");
    showHexDword(total);
    (void) Cconws("\n\r");
}

void showInt(int value, int length)
{
    char tmp[10];
    memset(tmp, 0, 10);

    if(length == -1) {                      // determine length?
        int i, div = 10;

        for(i=1; i<6; i++) {                // try from 10 to 1000000
            if((value / div) == 0) {        // after division the result is zero? we got the length
                length = i;
                break;
            }

            div = div * 10;                 // increase the divisor by 10
        }

        if(length == -1) {                  // length undetermined? use length 6
            length = 6;
        }
    }

    int i;
    for(i=0; i<length; i++) {               // go through the int lenght and get the digits
        int val, mod;

        val = value / 10;
        mod = value % 10;

        tmp[length - 1 - i] = mod + 48;     // store the current digit

        value = val;
    }

    (void) Cconws(tmp);                     // write it out
}

//--------------------------------------------------
// global variables, later used for calling hdIfCmdAsSuper
BYTE __readNotWrite, __cmdLength;
WORD __sectorCount;
BYTE *__cmd, *__buffer;

void hdIfCmdAsSuper(void)
{
    // this should be called through Supexec()
    (*hdIf.cmd)(__readNotWrite, __cmd, __cmdLength, __buffer, __sectorCount);
}

void hdIfCmdAsUser(BYTE readNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount)
{
    // store params to global vars
    __readNotWrite  = readNotWrite;
    __cmd           = cmd;
    __cmdLength     = cmdLength;
    __buffer        = buffer;
    __sectorCount   = sectorCount;    
    
    // call the function which does the real work, and uses those global vars
    Supexec(hdIfCmdAsSuper);
}

