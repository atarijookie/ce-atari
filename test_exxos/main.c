//--------------------------------------------------
#include <mint/osbind.h> 
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

BYTE readBuffer [2 * 512];
BYTE writeBuffer[2 * 512];
BYTE *rBuffer, *wBuffer;

void hdIfCmdAsUser(BYTE readNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount);
int  getIntFromUser(void);

#define SCSI_C_WRITE6                           0x0a
#define SCSI_C_READ6                            0x08

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

    (void) Cconws("\33pDestructive continous R/W test.\33q\r\n");
    (void) Cconws("This will corrupt data on drive\r\n");
    (void) Cconws("above 100 MB. Press 'c' to continue.\r\n");

    while(1) {
        key = Cnecin();
    
        if(key == 'c' || key == 'C') {
            break;
        }

        if(key == 'q' || key == 'Q') {
            return 0;
        }
    }

    //-------------

    (void) Cconws("Enter ACSI ID of device: ");
    BYTE acsiId = getIntFromUser();
    (void) Cconws("\r\n");

    //-------------
    // fill write buffer
    int i;
    for(i=0; i<512; i++) {
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

        cmd[4] = 1;                     // sector count
        cmd[5] = 0;                     // control byte

        wBuffer[0] = sector >> 16;      // store sector # at first bytes, so the data changes all the time
        wBuffer[1] = sector >>  8;
        wBuffer[2] = sector      ;

        sector++;                       // next time use next sector

        //-----------------------
        // WRITE DATA
        cmd[0] = (acsiId << 5) | SCSI_C_WRITE6;         // WRITE

        hdIfCmdAsUser(ACSI_WRITE, cmd, CMD_LENGTH_SHORT, wBuffer, 1);

        if(!hdIf.success || hdIf.statusByte != 0) {     // write failed?
            Cconout('W');
            continue;
        }

        //-----------------------
        // READ DATA
        cmd[0] = (acsiId << 5) | SCSI_C_READ6;          // read

        hdIfCmdAsUser(ACSI_READ, cmd, CMD_LENGTH_SHORT, rBuffer, 1);

        if(!hdIf.success || hdIf.statusByte != 0) {     // read failed?
            Cconout('R');
            continue;
        }

        //-----------------------
        // VERIFY DATA
        BYTE res = memcomp(wBuffer, rBuffer, 512);      // check data

        if(res == 0) {                                  // data OK?
            Cconout('*');
        } else {                                        // data fail?
            Cconout('x');
        }       
    }

    return 0;
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
    BYTE key;
    
    while(1) {
        key = Cnecin();

        if(key >= '0' && key <= '9') {
            break;
        }
    }
    
    Cconout(key);
    return (key - '0');
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
