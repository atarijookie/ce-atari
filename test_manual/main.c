//--------------------------------------------------
#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <support.h>

#include <stdint.h>
#include <stdio.h>

#include "../libacsiscsi/acsi.h"
#include "../libacsiscsi/scsi.h"
#include "../libacsiscsi/hdd_if.h"
#include "../libacsiscsi/stdlib.h"
#include "../libacsiscsi/global.h"
#include "../libacsiscsi/find_ce.h"
#include "translated.h"
#include "gemdos.h"


#define SCSI_C_READ10       0x28

//--------------------------------------------------

#define E_OK                0           // 00        = No error
#define E_CRC               0xfc        // -4 = 0xfc = CRC error

uint8_t deviceID;                          // bus ID from 0 to 7

void showHexByte (uint8_t val);
void showHexWord (uint16_t val);
void showHexDword(uint32_t val);

void scsi_reset(void);

void CEread(uint8_t verbose);
void sequentialWrite(void);

void CEwrite(void);
int  writeHansTest(int byteCount, uint16_t xorVal);
void showInt(int value, int length);
int getIntFromUser(uint8_t allowZero, uint8_t maxDigits);
int getIntFromUserMinMax(const char* message, int minV, int maxV);
char getHddInterfaceForTest(void);
void largeRead(void);
void SDread(void);

uint8_t showLogs = 1;
void showMenu(void);

#define RW_TEST_SIZE    MAXSECTORS

uint8_t  *pBufferOrig;
uint8_t  *pBuffer;
uint32_t memSizeBytes;
uint32_t memSizeSectors;

void hdIfCmdAsUser(uint8_t readNotWrite, uint8_t *cmd, uint8_t cmdLength, uint8_t *buffer, uint16_t sectorCount);

//-------------------------------------------------- 
#define MACHINE_ST      0
#define MACHINE_TT      2
#define MACHINE_FALCON  3

uint8_t machineType = MACHINE_ST;
char hddIface;

uint16_t tosVersion;
uint8_t tosVersionMajor;
uint8_t hddIf;

//--------------------------------------------------
int main(void)
{
    uint32_t scancode;
    uint32_t toEven;

    Clear_home();
    //                |                                        |             
    (void) Cconws("\33p>>>>>> Manual HDD test tool <<<<<<\33q\r\n\r\n");

    #define SIZE_13MB       (14 * 1024 * 1024)
    #define SIZE_4MB        ( 4 * 1024 * 1024)
    #define SIZE_MAXSECTORS (MAXSECTORS * 512)
    
    //-----------------------
    // get TOS version
    tosVersion = getTOSversion();
    tosVersionMajor = tosVersion >> 8;

    (void) Cconws("TOS version (major): ");
    showInt(tosVersionMajor, 1);
    (void) Cconws("\r\n");

    // ---------------------- 
    // get what machine we're running on
    machineType = Supexec(getMachineType);

    (void) Cconws("Running on: ");
    switch(machineType) {
        case MACHINE_ST:        (void) Cconws("ST\r\n"); break;
        case MACHINE_TT:        (void) Cconws("TT\r\n"); break;
        case MACHINE_FALCON:    (void) Cconws("Falcon\r\n"); break;
        default:                (void) Cconws("???\r\n"); break;
    }

    //-----------------------
    // get which HDD interface to user
    hddIface = getHddInterfaceForTest();

    if(hddIface == 'a') {   // using ACSI? we need only 128 kB of RAM
        memSizeSectors = MAXSECTORS;
        memSizeBytes = SIZE_MAXSECTORS;
    } else {                // using SCSI? we can transfer megs, ask for allocation size
        uint32_t memInMBs = getIntFromUserMinMax("How much MBs of RAM to allocate (1-13): ", 1, 13);
        memSizeBytes = (memInMBs * (1024*1024)) + 2;    // MBs to bytes
        memSizeSectors = memSizeBytes / 512;            // bytes to sectors
    }

    (void) Cconws("Will try to alloc   : ");
    showHexDword(memSizeBytes);
    (void) Cconws("\r\n");

    if(machineType == MACHINE_ST) {     // use Malloc() on ST
        pBufferOrig = (uint8_t *) Malloc(memSizeBytes);
    } else {                            // use Mxalloc on TT/Falcon, force to ST RAM
        pBufferOrig = (uint8_t *) Mxalloc(memSizeBytes, 0);
    }

    if(pBufferOrig == NULL) {
        (void) Cconws("Failed to allocate memory!\r\n");
        (void) Cconws("Press any key to terminate.\r\n");
        
        Cnecin();
        return 0;
    }
    
    // ---------------------- 
    // create buffer pointer to even address 
    toEven = (uint32_t) &pBufferOrig[0];

    if(toEven & 0x0001) {       // not even number? 
        toEven++;
        memSizeBytes--;
    }

    pBuffer = (uint8_t *) toEven; 

    // ---------------------- 
    // search for device on the ACSI / SCSI bus 
    deviceID = 0;
    
    (void) Cconws("HDD Interface       : ");
    
    switch(hddIface) {
        case 'a':   
            (void) Cconws("\33pACSI\33q");
            hdd_if_select(IF_ACSI);
            hddIf = IF_ACSI;
            deviceID = 0;           // ACSI ID 0
            break;

        case 't':
            (void) Cconws("\33pTT SCSI\33q");
            hdd_if_select(IF_SCSI_TT);
            hddIf = IF_SCSI_TT;
            deviceID = 0;           // SCSI ID 0
            break;

        case 'f':
            (void) Cconws("\33pFalcon SCSI\33q");
            hdd_if_select(IF_SCSI_FALCON);
            hddIf = IF_SCSI_FALCON;
            deviceID = 1;           // SCSI ID 1
            
            break;
    } 
    (void) Cconws("\r\n");

    showLogs = 0;                   // turn off logs - there will be errors on findDevice when device doesn't exist 
    uint8_t res = findDevice(FIND_DEV_CE | FIND_DEV_CS);

    if(res == DEVICE_NOT_FOUND) {
		sleep(3);
        return 0;
    }

    deviceID = res & 0x07;                                  // store the BUS ID of device
    showLogs = 1;                   // turn on logs
            
    showMenu();

    //-----------------
    // main menu loop
    while(1) {
        scancode    = Bconin(DEV_CONSOLE); 		                    // get char form keyboard, no echo on screen 
        char key    = scancode & 0xff;

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
            cs_inquiry(deviceID, hddIf);
            continue;
        }

        if(key == 'I') {
            int i;
            uint32_t start, end, diff;

            start = getTicks();
            for(i=0; i<10; i++) {
                cs_inquiry(deviceID, hddIf);
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
            uint32_t start, end, diff;

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

        if(key == 'W') {            // sequential write
            sequentialWrite();
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
    (void) Cconws("W - sequential write RAW device\r\n");
    (void) Cconws("L - large read\r\n");
    (void) Cconws("f - Find device on SCSI\r\n");
    (void) Cconws("c - clear screen\r\n");
    (void) Cconws("Q - quit\r\n\r\n");
}

uint8_t commandLong[CMD_LENGTH_LONG] = {0x1f,	0, 'C', 'E', HOSTMOD_TRANSLATED_DISK, 0, 0, 0, 0, 0, 0, 0, 0}; 
int readHansTest(int byteCount, uint16_t xorVal, uint8_t verbose);

void CEread(uint8_t verbose)
{
  	commandLong[0] = (deviceID << 5) | 0x1f;			// cmd[0] = ACSI_id + ICD command marker (0x1f)	
	commandLong[1] = 0xA0;								// cmd[1] = command length group (5 << 5) + TEST UNIT READY (0)  	

    uint16_t xorVal=0xC0DE;
    
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
    uint32_t mbCount       = (memSizeSectors      >> 11);   // sectors to mega bytes
    uint32_t timeoutSecs   = mbCount * 3;                          // mega bytes to seconds
    
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
    
    commandLong[8] = (uint8_t) (memSizeSectors >> 8);
    commandLong[9] = (uint8_t) (memSizeSectors     );
    
    hdIfCmdAsUser(1, commandLong, 11, pBuffer, memSizeSectors);

    (void) Cconws("Command success: ");
    showHexByte(hdIf.success);
    (void) Cconws("\r\n");
    
    (void) Cconws("SCSI result    : ");
    showHexByte(hdIf.statusByte);
    (void) Cconws("\r\n");
}

int readHansTest(int byteCount, uint16_t xorVal, uint8_t verbose)
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
    uint16_t counter = 0;
    uint16_t data = 0;
    for(i=0; i<byteCount; i += 2) {
        data = counter ^ xorVal;       // create word
        if( !(pBuffer[i]==(data>>8) && pBuffer[i+1]==(data&0xFF)) ){
          return -2;
        }  
        counter++;
    }

    if(byteCount & 1) {                                 // odd number of bytes? add last byte
        uint8_t lastByte = (counter ^ xorVal) >> 8;
        if( pBuffer[byteCount-1]!=lastByte ){
          return -2;
        }  
    }
    
	return 0;
}

void SDread(void)
{
    uint8_t cmd[6];
    
    memset(cmd, 0, 6);
    cmd[0] = (deviceID << 5) | SCSI_CMD_READ6;
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

        cmd[0] = (deviceID << 5) | SCSI_CMD_REQUEST_SENSE;
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

//--------------------------------------------
uint8_t cs_inquiry2(uint8_t id)
{
    uint8_t cmd[CMD_LENGTH_SHORT];
    
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
uint8_t scsiWrite(uint8_t devId, uint32_t sectorStart, uint8_t sectorCount, uint8_t* data)
{
    uint8_t cmd[6];
    cmd[0] = (devId << 5) | SCSI_CMD_WRITE6;
    cmd[1] = (sectorStart >> 16);
    cmd[2] = (sectorStart >>  8);
    cmd[3] = (sectorStart      );
    cmd[4] = sectorCount;
    cmd[5] = 0;

    hdIfCmdAsUser(ACSI_WRITE, cmd, 6, data, sectorCount);
    
    if(!hdIf.success) {
        return 0xff;
    }
    return hdIf.statusByte;
}

void sequentialWrite(void)
{
    // show initial message
    (void) Cconws("\r\nSequential write to RAW device.\r\n");

    uint8_t testDevId = getIntFromUserMinMax("Choose device ID (0-7): ", 0, 7);

    // fill buffer with data
    int j;
    for(j=0; j<(MAXSECTORS * 512); j++) {
        pBuffer[j] = j;
    }

    uint32_t writeMBs = getIntFromUserMinMax("How much MBs to write (1-1024): ", 1, 1024);
    uint32_t writeBuffers = (writeMBs * 1024 * 1024) / (MAXSECTORS * 512);  // MBs to bytes, bytes to count of 127 kB buffers

    //---------------
    // if we got at least TOS 2.00, we can also use DMAwrite(), for older use only our own routine
    uint8_t ownRoutine = 0;

    if(tosVersionMajor >= 2) {
        (void) Cconws("Choose HDD access routine:\r\n");

        while(1) {
            (void) Cconws("\33pC\33q - custom routine\r\n");
            (void) Cconws("\33pD\33q - DMAwrite() from TOS\r\n");

            char key = Cnecin();
            if(key >= 'A' && key <= 'Z') {
                key += 32;
            }
            
            if(key == 'c') {    // custom routine?
                ownRoutine = 1;
                break;
            }

            if(key == 'd') {    // tos routine?
                ownRoutine = 0;
                break;
            }
        }

        if(!ownRoutine && hddIface != 'a') {    // if using TOS routine and using SCSI interface
            testDevId += 8;                     // SCSI device ID for DMAwrite is 8-15 (ACSI device ID for DMAwrite is 0-7)
        }
    }

    (void) Cconws("Using ");
    if(ownRoutine) {
        (void) Cconws("custom routine\r\n");
    } else {
        (void) Cconws("DMAwrite() from TOS\r\n");
    }

    //---------------
    // write data to device in a loop
    uint32_t i;
    uint32_t start = 0;
    for(i=0; i<writeBuffers; i++) {
        if(i % 40 == 0) {   // every 5 MB
            (void) Cconws("\r\n");
            int megs = i / 8;
            showInt(megs, 3);
            (void) Cconws(" of ");
            showInt(writeMBs, 3);
            (void) Cconws(" MB: ");
        }

        uint8_t res;
        
        if(ownRoutine) {
            res = scsiWrite(testDevId, start, MAXSECTORS, pBuffer);             // returns status byte, 0 means success
        } else {
            res = (uint8_t) DMAwrite(start, MAXSECTORS, pBuffer, testDevId);    // returns 0 on success, negative value on fail
        }

        start += MAXSECTORS;    // advance start to next position
        if(res == 0) {
            (void) Cconws("*");
        } else {
            (void) Cconws("-");
        }
    }

    (void) Cconws("\r\nDone.\r\nPress key to continue.\r\n");   // show message
    Cnecin();   // wait for key
    showMenu(); // show menu
}

void CEwrite(void)
{
    commandLong[0] = (deviceID << 5) | 0x1f;			// cmd[0] = ACSI_id + ICD command marker (0x1f)	
    commandLong[1] = 0xA0;								// cmd[1] = command length group (5 << 5) + TEST UNIT READY (0)  	

    uint16_t xorVal=0xC0DE;
    
    int res = writeHansTest(RW_TEST_SIZE * 512, xorVal);
    
    switch( res )
    {
        case -1:    (void) Cconws("TIMEOUT\r\n");   break;
        case -2:    (void) Cconws("CRC FAIL\r\n");  break;
        case 0:     (void) Cconws("GOOD\r\n");      break;
        default:    (void) Cconws("ERROR\r\n");     break;
    }
}
        
int writeHansTest(int byteCount, uint16_t xorVal)
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
    uint16_t counter = 0;
    uint16_t data = 0;
    for(i=0; i<byteCount; i += 2) {
        data = counter ^ xorVal;       // create word
        pBuffer[i] = (data>>8);
        pBuffer[i+1] = (data&0xFF);
        counter++;
    }

    if(byteCount & 1) {                                 // odd number of bytes? add last byte
        uint8_t lastByte = (counter ^ xorVal) >> 8;
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

int getIntFromUser(uint8_t allowZero, uint8_t maxDigits)
{
    int intValue    = 0;
    int gotDigits   = 0;
    
    while(1) {
        uint8_t key = Cnecin();

        if((key == 13 && gotDigits > 0) ||      // it's enter and got at least 1 digit, quit
            gotDigits >= maxDigits) {           // if got maxDigits digits count, quit
            
            if(intValue > 0) {                  // if the entered number is greated than 0 (to avoid typing '000'), return it, otherwise ignore it
                (void) Cconws("\r\n");
                return intValue;
            }
        }
        
        if(key < '0' || key > '9') {            // out of char range? try again
            continue;
        }
        
        if(!allowZero && key == '0') {          // if zero is not allowed, and it's zero, try again
            continue;
        }

        Cconout(key);                           // show the digit
        int digit = key - '0';                  // get digit from char
        
        intValue = (intValue * 10) + digit;     // append new digit
        gotDigits++;
    }
}

char getHddInterfaceForTest(void)
{
    if(machineType == MACHINE_ST) {         // it's a ST? always ACSI
        return 'a';
    }

    if(machineType == MACHINE_FALCON) {     // if it's Falcon, use Falcon's SCSI
        return 'f';
    }

    // We're on TT, we need to ask user
    (void) Cconws("Running on TT, choose HDD interface:\r\n");
    
    while(1) {
        (void) Cconws("'A' - ACSI \r\n");
        (void) Cconws("'S' - SCSI \r\n");

        char key = Cnecin();
        if(key >= 'A' && key <= 'Z') {
            key += 32;
        }
        
        if(key == 'a') {    // user selected ACSI
            return 'a';
        }

        if(key == 's') {    // user selected SCSI, return 't' (means TT's SCSI)
            return 't';
        }
    }
}

int getIntFromUserMinMax(const char* message, int minV, int maxV)
{
    (void) Cconws(message);

    int val = 0;
    while(1) {
        val = getIntFromUser(1, 5);

        if(val >= minV && val <= maxV) {
            return val;
        }
        (void) Cconws("\r\n");
    }

    return 0;
}
