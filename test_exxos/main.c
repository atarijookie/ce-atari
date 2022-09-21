//--------------------------------------------------
#include <mint/osbind.h> 
#include <mint/sysvars.h>
#include <mint/linea.h> 
#include <stdio.h>

#include "acsi.h"
#include "gemdos.h"
#include "gemdos_errno.h"
#include "VT52.h"
#include "version.h"
#include "hdd_if.h"
#include "stdlib.h"

//--------------------------------------------------

void sleep(int seconds);

void showHexByte(BYTE val);
void showHexDword(DWORD val);

//--------------------------------------------------
BYTE deviceID;

BYTE readBuffer [254 * 512 + 4];
BYTE writeBuffer[254 * 512 + 4];
BYTE *rBuffer, *wBuffer;
uint16_t* pZeroInTos;
uint8_t readZerosWhileWait;

void hdIfCmdAsUser(BYTE readNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount);
int  getIntFromUser(void);

#define SCSI_C_WRITE6                           0x0a
#define SCSI_C_READ6                            0x08

void writeReadAbove100MB(BYTE acsiId);
void justWriteKnownData256sectors(BYTE acsiId);

static long get_tos_header(void)
{
    return *_sysbase;
}

void findAddressToZeroInTos(void)
{
    OSHEADER* tos_header;
    uintptr_t tos_base;
    uint16_t tos_version;
    size_t tos_size = 0;

    // get TOS pointer and size retrieving - taken from https://github.com/mikrosk/uDump/blob/master/udump.c
    tos_header = (OSHEADER*)(Supexec(get_tos_header));
    tos_base = (uintptr_t)tos_header;
    tos_version = tos_header->os_version;

    if (tos_version < 0x0106) {
        tos_size = 192 * 1024;
    } else {
        tos_size = 256 * 1024;
    }

    // find location in TOS with zero value
    pZeroInTos = (uint16_t*) tos_base;
    uint8_t found = 0;
    size_t i;
    for(i=0; i<tos_size; i++) {         // go through the TOS addresses
        uint16_t val = *pZeroInTos;     // read value from TOS

        if(val == 0) {                  // it's 0 as we wanted, good
             (void) Cconws("TOS location with zero found.\r\n");
             found = 1;
             break;
        }

        pZeroInTos++;                   // to next address
    }

    // it zero not found in TOS, show warning
    if(!found) {
        (void) Cconws("TOS location with zero NOT found!\r\n");
        pZeroInTos = (uint16_t*) tos_base;
    }
}

//--------------------------------------------------
int main(void)
{
    BYTE key;
    DWORD toEven;

    //----------------------
    // read all the keys which are waiting, so we can ignore them
    while(1) {
        if(Cconis() != 0) {             // if some key is waiting, read it
            Cnecin();
        } else {                        // no key is waiting, quit the loop
            break;
        }
    }

    //----------------------
    hdd_if_select(IF_ACSI);
    hdIf.maxRetriesCount = 0;

    // ---------------------- 
    // create buffer pointer to even address 
    toEven = (DWORD) &readBuffer[0];

    if(toEven & 0x0001)       // not even number? 
        toEven++;

    rBuffer = (BYTE *) toEven; 

    //----------
    toEven = (DWORD) &writeBuffer[0];

    if(toEven & 0x0001)       // not even number? 
        toEven++;

    wBuffer = (BYTE *) toEven; 

    Clear_home();
    VT52_Wrap_on();

    findAddressToZeroInTos();

    (void) Cconws("\33pDestructive continous R/W test.\33q\r\n");
    (void) Cconws("This will corrupt data on drive!!!\r\n");
    (void) Cconws("Press 'c' to continue.\r\n\r\n");

    while(1) {
        key = Cnecin();

        if(key == 'c' || key == 'C') {
            break;
        }

        if(key == 'q' || key == 'Q') {
            return 0;
        }
    }

    (void) Cconws("To quit any time, press Ctrl+C\r\n\r\n");
    
    //-------------

    (void) Cconws("Enter ACSI ID of device          : ");
    BYTE acsiId = getIntFromUser();
    (void) Cconws("\r\n");

    //-------------
    (void) Cconws("\r\n\33pChoose test type :\33q\r\n");
    (void) Cconws("\33pL\33q - write-read-verify loop\r\n");
    (void) Cconws("\33pZ\33q - like L but reading 0s during wait\r\n");
    (void) Cconws("\33pC\33q - fill first 256 sectors with counter\r\n");
    (void) Cconws("\33pQ\33q - quit\r\n\r\n");

    while(1) {
        key = Cnecin();

        if(key == 'l' || key == 'L') {
            readZerosWhileWait = 0;
            writeReadAbove100MB(acsiId);
            break;
        }

        if(key == 'z' || key == 'Z') {
            readZerosWhileWait = 1;
            writeReadAbove100MB(acsiId);
            readZerosWhileWait = 0;
            break;
        }

        if(key == 'c' || key == 'C') {
            readZerosWhileWait = 0;
            justWriteKnownData256sectors(acsiId);
            break;
        }

        if(key == 'q' || key == 'Q') {
            break;
        }
    }
    
    return 0;
}

void justWriteKnownData256sectors(BYTE acsiId)
{
    //-------------
    // fill write buffer
    int i;
    for(i=0; i<512; i++) {                              // set buffer to counter values
        wBuffer[i] = i;
    }

    BYTE  cmd[6];
    DWORD sector;

    for(sector=0; sector<256; sector++) {
        cmd[0] = (acsiId << 5) | SCSI_C_WRITE6;         // WRITE

        cmd[1]      = sector >> 16;                     // sector number
        cmd[2]      = sector >>  8;
        cmd[3]      = sector      ;

        cmd[4]      = 1;                                // only 1 sector 
        cmd[5]      = 0;                                // control byte

        wBuffer[0]  = sector >> 16;                     // store sector # at first bytes, so the data changes all the time
        wBuffer[1]  = sector >>  8;
        wBuffer[2]  = sector      ;

        // WRITE DATA
        hdIfCmdAsUser(ACSI_WRITE, cmd, CMD_LENGTH_SHORT, wBuffer, 1);

        if(!hdIf.success || hdIf.statusByte != 0) {     // write failed?
            Cconout('W');
        } else {                                        // write OK?
            Cconout('*');
        }
    }
    
    (void) Cconws("\r\nDone. Press any key to quit.\r\n");
    Cnecin();
}

void writeReadAbove100MB(BYTE acsiId)
{
    int sectorCount;
    while(1) {
        (void) Cconws("Enter sector count per op (1-254): ");
        sectorCount = getIntFromUser();                     // how many sectors
        (void) Cconws("\r\n\r\n");
        
        if(sectorCount >= 1 && sectorCount <= 254) {
            break;
        }
    }

    int byteCount   = sectorCount * 512;                    // how many bytes
    
    //-------------
    // fill write buffer
    int i;
    for(i=0; i<byteCount; i++) {        // set buffer to counter values
        wBuffer[i] = i;
    }

    DWORD sectorStart   = 0x032000;     // starting sector: at 100 MB
    DWORD sectorEnd     = 0x1FFFFF;     // ending   sector: at   1 GB

    DWORD sector        = sectorStart;
    BYTE cmd[6];

    while(1) {
        if(sector >= sectorEnd) {       // if doing last sector
            sector = sectorStart;       // go to starting sector
        }

        cmd[1] = sector >> 16;          // sector number
        cmd[2] = sector >>  8;
        cmd[3] = sector      ;

        cmd[4] = sectorCount;           // sector count
        cmd[5] = 0;                     // control byte

        wBuffer[0] = sector >> 16;      // store sector # at first bytes, so the data changes all the time
        wBuffer[1] = sector >>  8;
        wBuffer[2] = sector      ;

        sector++;                       // next time use next sector

        //-----------------------
        // WRITE DATA
        cmd[0] = (acsiId << 5) | SCSI_C_WRITE6;         // WRITE

        hdIfCmdAsUser(ACSI_WRITE, cmd, CMD_LENGTH_SHORT, wBuffer, sectorCount);

        if(!hdIf.success || hdIf.statusByte != 0) {     // write failed?
            Cconout('W');
            continue;
        }

        //-----------------------
        // READ DATA
        cmd[0] = (acsiId << 5) | SCSI_C_READ6;          // read

        memset(rBuffer, 0, byteCount);                  // clear the read buffer, just to be sure...

        hdIfCmdAsUser(ACSI_READ, cmd, CMD_LENGTH_SHORT, rBuffer, sectorCount);

        if(!hdIf.success || hdIf.statusByte != 0) {     // read failed?
            Cconout('R');
            continue;
        }

        //-----------------------
        // VERIFY DATA
        BYTE res = memcomp(wBuffer, rBuffer, byteCount);// check data

        if(res == 0) {                                  // data OK?
            Cconout('*');
        } else {                                        // data fail?
            Cconout('x');
        }       
    }
}

BYTE showQuestionGetBool(const char *question, BYTE trueKey, const char *trueWord, BYTE falseKey, const char *falseWord)
{
    // show question
    (void) Cconws(question);

    while(1) {
        BYTE key = Cnecin();

        if(key >= 'A' && key <= 'Z') {          // upper case letter? to lower case
            key += 32;
        }

        if(key == trueKey) {                    // yes
            (void) Cconws(" ");
            (void) Cconws(trueWord);
            (void) Cconws("\r\n");
            return 1;
        }

        if(key == falseKey) {                   // no
            (void) Cconws(" ");
            (void) Cconws(falseWord);
            (void) Cconws("\r\n");
            return 0;
        }
    }
}

int getIntFromUser(void)
{
    int value = 0;

    BYTE key;
    
    while(1) {
        key = Cnecin();

        if(key == 13) {                     // enter? quit!
            break;
        }
        
        if(key < '0' && key > '9') {        // invalid number? 
            continue;
        }
        
        Cconout(key);                       // show it
        
        value = value * 10;
        value = value + (key - '0');        // add current number to total value
    }
    
    return value;
}

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

void showHexDword(DWORD val)
{
    showHexByte((BYTE) (val >> 24));
    showHexByte((BYTE) (val >> 16));
    showHexByte((BYTE) (val >>  8));
    showHexByte((BYTE)  val);
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
