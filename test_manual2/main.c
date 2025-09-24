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

#define E_OK                0           // 00        = No error
#define E_CRC               0xfc        // -4 = 0xfc = CRC error

uint8_t deviceID;                          // bus ID from 0 to 7

void showHexByte (uint8_t val);
void showHexWord (uint16_t val);
void showHexDword(uint32_t val);

void scsi_reset(void);

void showInt(int value, int length);
int getIntFromUser(uint8_t allowZero, uint8_t maxDigits);
int getIntFromUserMinMax(const char* message, int minV, int maxV);
char getHddInterfaceForTest(void);
void largeRead(void);

uint8_t showLogs = 1;
void showMenu(void);

#define RW_TEST_SIZE    MAXSECTORS

uint8_t  *pBufferOrig;
uint8_t  *pBuffer;
uint32_t memSizeBytes;
uint32_t memSizeMBs;
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
        memSizeMBs = 0;
        memSizeBytes = SIZE_MAXSECTORS;
        memSizeSectors = MAXSECTORS;
    } else {                // using SCSI? we can transfer megs, ask for allocation size
        memSizeMBs = getIntFromUserMinMax("How much MBs of RAM to allocate (1-13): ", 1, 13);
        memSizeBytes = (memSizeMBs * (1024*1024)) + 2;    // MBs to bytes
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

    //--------------------
    // let user manually specify device ID
    deviceID = getIntFromUserMinMax("Select device ID (0-7): ", 0, 7);
    showLogs = 1;                   // turn on logs

    showMenu();

    //-----------------
    // main menu loop
    while(1) {
        scancode = Bconin(DEV_CONSOLE); 		                    // get char form keyboard, no echo on screen 
        char key = scancode & 0xff;

        if(key == 'q' || key == 'Q') {
            (void) Cconws("Terminating...\r\n");
            sleep(1);
            break;
        }

        if(key == 'x' || key == 'X') {
            (void) Cconws("SCSI reset...");
            scsi_reset();
            (void) Cconws("done\r\n");
            continue;
        }

        if(key == 'l' || key == 'L') {
            largeRead();
        }

        if(key == 'c' || key == 'C') {
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
    
    (void) Cconws("X - SCSI reset\r\n");
    (void) Cconws("L - large read\r\n");
    (void) Cconws("C - clear screen\r\n");
    (void) Cconws("Q - quit\r\n\r\n");
}

uint8_t commandLong[CMD_LENGTH_LONG] = {0x1f,	0, 'C', 'E', HOSTMOD_TRANSLATED_DISK, 0, 0, 0, 0, 0, 0, 0, 0}; 

void largeRead(void)
{
    if(memSizeMBs < 1) {
        (void) Cconws("Not enough RAM allocated for large read.\r\n");
        return;
    }

    uint32_t testSizeMBs = getIntFromUserMinMax("How much MBs should be transferred: ", 1, memSizeMBs);
    uint32_t testSizeSectors = (testSizeMBs * 1024 * 1024) / 512;   // test size from MBs to count of sectors

    uint32_t timeoutSecs = testSizeMBs * 3;       // mega bytes to seconds

    (void) Cconws("READ(10) - dev: ");
    showInt(deviceID, 1);
    (void) Cconws(", size: ");
    showInt(testSizeMBs, 2);
    (void) Cconws(" MB, timeout: ");
    showInt(timeoutSecs, 2);
    (void) Cconws(" s\r\n");

    memset(commandLong, 0, sizeof(commandLong));

    commandLong[0] = (deviceID << 5) | 0x1f;
    commandLong[1] = SCSI_C_READ10;
    
    commandLong[8] = (uint8_t) (testSizeSectors >> 8);
    commandLong[9] = (uint8_t) (testSizeSectors     );
    
    hdIfCmdAsUser(1, commandLong, 11, pBuffer, testSizeSectors);

    (void) Cconws("Command success: ");
    showHexByte(hdIf.success);
    (void) Cconws("\r\n");

    (void) Cconws("SCSI result    : ");
    showHexByte(hdIf.statusByte);
    (void) Cconws("\r\n");
}

//--------------------------------------------

int getIntFromUser(uint8_t allowZero, uint8_t maxDigits)
{
    int intValue    = 0;
    int gotDigits   = 0;
    
    while(1) {
        uint8_t key = Cnecin();

        if((key == 13 && gotDigits > 0) ||      // it's enter and got at least 1 digit, quit
            gotDigits >= maxDigits) {           // if got maxDigits digits count, quit
            (void) Cconws("\r\n");
            return intValue;
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
