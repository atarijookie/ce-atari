// vim: expandtab shiftwidth=4 tabstop=4
#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <support.h>

#include <stdint.h>
#include <stdio.h>

#include "stdlib.h"
#include "hdd_if.h"
#include "translated.h"
#include "vt52.h"
#include "main.h"

// ------------------------------------------------------------------
WORD tosVersion;
void getTosVersion(void);

THDif *hdIf;
void getCE_API(void);

WORD dmaBuffer[DMA_BUFFER_SIZE/2];  // declare as WORD buffer to force WORD alignment
BYTE *pDmaBuffer;

BYTE commandShort[CMD_LENGTH_SHORT] = {         0, 'C', 'E', HOSTMOD_TRANSLATED_DISK, 0, 0};
BYTE commandLong [CMD_LENGTH_LONG]  = {0x1f, 0xA0, 'C', 'E', HOSTMOD_TRANSLATED_DISK, 0, 0, 0, 0, 0, 0, 0, 0};

void hdIfCmdAsUser(BYTE readNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount);
void showHexWord  (WORD val);

// ------------------------------------------------------------------
typedef struct {
    DWORD maxSizeMB;
    DWORD maxSizeSectors;

    // pc boot sector values
    BYTE  pSectorsPerCluster;
    WORD  pReservedSectors;
    WORD  pSectorsPerFat;
    WORD  pSectorsPerTrack;
    WORD  pNumberOfHeads;
    DWORD pHiddenSectors;

    // atari boot sector values
    WORD  aBytesPerSector;
    BYTE  aSectorsPerCluster;
    WORD  aSectorsPerFat;
} TBpb;

typedef struct {
    DWORD bootsector;
    DWORD fat1;
    DWORD fat2;
    DWORD rootDirEntry;
} PcSectorsPosition;

TBpb bpb0255 = { 256, 0x07F800,   0x10, 0x0009, 0x0040, 0x003a, 0x0009, 0x003a,   0x2000, 0x02, 0x0004};
TBpb bpb0511 = { 512, 0x0FF800,   0x20, 0x0011, 0x0080, 0x003e, 0x0011, 0x003e,   0x2000, 0x01, 0x0008};
TBpb bpb1023 = {1024, 0x1FF800,   0x40, 0x0021, 0x0080, 0x003f, 0x0021, 0x003f,   0x4000, 0x02, 0x0004};

DWORD serialNumber;                 // generated serial number of the drive

//--------------------------
// DEVTYPE values really sent from CE
#define DEVTYPE_OFF         0
#define DEVTYPE_SD          1
#define DEVTYPE_RAW         2
#define DEVTYPE_TRANSLATED  3

// DEVTYPE values used in this app for making other states
#define DEVTYPE_NOTHING     10      // nothing   responded on this ID
#define DEVTYPE_UNKNOWN     11      // something responded on this ID, but it's not CE

BYTE devicesOnBus[8];               // contains DEVTYPE_ for all the xCSI IDs
void scanXCSIbus        (void);
BYTE getCEid            (BYTE anyCEnotOnlySD);
BYTE getSDcardInfo      (BYTE deviceID);
void showSDcardCapacity (void);
BYTE enableCEids        (BYTE ceId);

BYTE readBootsectorAndIdentifyContent   (void);
BYTE writeBootAndOtherSectors_likeHddriver  (TBpb *partParams);
BYTE writeBootAndOtherSectors_likeWin7      (TBpb *partParams, PcSectorsPosition pcSectPos);

void showBpb(void);

struct {
    BYTE  id;                       // assigned ACSI ID
    BYTE  isInit;                   // contains if the SD card is present and initialized
    DWORD SCapacity;                // capacity of the card in sectors
} SDcard;

// ------------------------------------------------------------------
int main( int argc, char* argv[] )
{
    memset(devicesOnBus, 0, sizeof(devicesOnBus));
    pDmaBuffer = (BYTE *) dmaBuffer;
    
    TBpb *partParams;
    
    // write some header out
    Clear_home();
    //                |                                        |
    (void) Cconws("\33p   [ CosmosEx SD NOOB configurator ]    \33q\r\n\r\n");

    //            |                                        |
    (void) Cconws("This tool is for inexperienced users and\r\n");
    (void) Cconws("it will automatically partition your    \r\n");
    (void) Cconws("SD card to maximum size allowed on your \r\n");
    (void) Cconws("TOS version, making it DOS compatible   \r\n");
    (void) Cconws("and accessible on ACSI bus.             \r\n");
    (void) Cconws("\r\n");
    (void) Cconws("If you know how to do this by yourself, \r\n");
    (void) Cconws("then you might achieve better results   \r\n");
    (void) Cconws("doing so than with this automatic tool. \r\n");
    (void) Cconws("\r\n");
    (void) Cconws("Press \33p[Q]\33q to quit.\r\n");
    (void) Cconws("Press \33p[C]\33q to continue.\r\n");

    while(1) {
        BYTE key = Cnecin();

        if(key == 'q' || key == 'Q') {  // quit?
            return 0;
        }

        if(key == 'c' || key == 'C') {  // continue?
            break;
        }
    }

    //-------------
    // if user decided to continue...
    Clear_home();
    (void) Cconws("\33p   [ CosmosEx SD NOOB configurator ]    \33q\r\n\r\n");
    Supexec(getTosVersion);             // find out TOS version

    //            |                                        |
    // show TOS version
    (void) Cconws("Your TOS version      : ");
    showInt((tosVersion >>  8) & 0x0f, 1);
    (void) Cconws(".");
    showInt((tosVersion >>  4) & 0x0f, 1);
    showInt((tosVersion      ) & 0x0f, 1);
    (void) Cconws("\r\n");

    // show maximum partition size
    (void) Cconws("Maximum partition size: ");

    if(tosVersion <= 0x0102) {          // TOS 1.02 and older
        partParams = &bpb0255;
        (void) Cconws("256 MB\r\n");
    } else if(tosVersion < 0x0400) {    // TOS 1.04 - 3.0x
        partParams = &bpb0511;
        (void) Cconws("512 MB\r\n");
    } else {                            // TOS 4.0x
        partParams = &bpb1023;
        (void) Cconws("1024 MB\r\n");
    } 

    //-------------
    // find CE_DD API in cookie jar
    Supexec(getCE_API);

    (void) Cconws("CosmosEx DD API       : ");
    if(hdIf) {                                  // if CE_DD API was found, good
        (void) Cconws("found\r\n");
    } else {                                    // if CE_DD API wasn't found, fail
        (void) Cconws("not found\r\n\r\n");
        (void) Cconws("\33pPlease run CE_DD.PRG before this tool!\33q\r\n");
        (void) Cconws("Press any key to terminate...\r\n");

        Cnecin();
        return 0;
    }

    BYTE res;

#define WITHPASTI
    
#ifndef WITHPASTI
    //-------------
    // Scan xCSI bus to find CE and CE_SD.
    // Use solo command to get if SD card is inserted, and its capacity.
    BYTE ceId;
    
    scanXCSIbus();                  // scan xCSI bus and find everything (even non-CE devices)
    ceId = getCEid(TRUE);           // get first ID which belongs to CE

    if(ceId == 0xff) {              // no CE ID found? quit, fail
        (void) Cconws("\r\nCosmosEx was not found on bus.\r\n");
        (void) Cconws("Press any key to terminate...\r\n");

        Cnecin();
        return 0;
    }

    while(1) {
        res = getSDcardInfo(ceId);  // try to get the info about the card

        if(!res) {                  // failed to get the info about the card? fail
            (void) Cconws("\r\nFailed to get card info, fail.\r\n");
            (void) Cconws("Press any key to terminate...\r\n");

            Cnecin();
            return 0;
        }

        (void) Cconws("SD card is inserted   : ");

        if(SDcard.isInit) {             // if got the card, we can quit
            (void) Cconws("YES\r\n");

            DWORD capacityMB = SDcard.SCapacity >> 11;
            if(capacityMB < partParams->maxSizeMB) {
                (void) Cconws("SD card is smaller than max partition.\r\n");
                (void) Cconws("Please insert larger SD card.\r\n");
                
                Cnecin();
                VT52_Del_line();    VT52_Cur_up();
                VT52_Del_line();    VT52_Cur_up();
                VT52_Del_line();
                continue;                
            }

            break;
        }

        // don't have the card? try again
        (void) Cconws("NO\r\n");
        (void) Cconws("Please insert the SD card in slot.\r\n");

        sleep(1);

        VT52_Del_line();        // delete 2nd line
        VT52_Cur_up();          // go line up
        VT52_Del_line();        // delete 1st line
        VT52_Cur_up();          // go line up
    }

    (void) Cconws("SD card capacity      : ");
    showSDcardCapacity();

    //-------------
    // the following function should go through bus settings, fix ID order (if it's wrong), assing / enable IDs if they are not enabled
    res = enableCEids(ceId);

    if(!res) {
        (void) Cconws("\r\nPress any key to terminate...\r\n");

        Cnecin();
        return 0;
    }

    // show SD ID to user
    (void) Cconws("SD ID on bus          : ");
    showInt(SDcard.id, 1);
    (void) Cconws("\r\n");

#else
    // when running with PASTI
    SDcard.id   = 0;
#endif    
    
    //-------------
    // read boot sector from SD card and show what's in it
    res = readBootsectorAndIdentifyContent();

    if(!res) {
        (void) Cconws("\r\nPress any key to terminate...\r\n");

        Cnecin();
        return 0;
    }

    //-------------
    // warn user that if he will proceed, he will loose data
    serialNumber = Supexec(getTicks);       // current 200 Hz timer value will be used as serial number

    (void) Cconws("\r\n");
    //                |                                        |
    (void) Cconws("\33p   [      Point of no return       ]    \33q\r\n");

    //            |                                        |
    (void) Cconws("We're ready to partition your card.      \r\n");
    (void) Cconws("If you will proceed further, you will    \r\n");
    (void) Cconws("loose any existing data on the SD card.  \r\n");
    (void) Cconws("To continue, type \33pYES\33q (or \33pQ\33q to quit).\r\n");

    while(1) {
        (void) Cconws("\r\nYour choice           : ");

        char answer[12];
        answer[0] = sizeof(answer - 1);     // how much characters can be entered
        Cconrs(answer);

        if(answer[1] == 0) {                // nothing entered? try again
            continue;
        }

        if(answer[1] == 1 && (answer[2] == 'q' || answer[2] == 'Q')) {  // quit instead of write? 
            (void) Cconws("\r\n\r\nNothing was written to card and quitting\r\n");
            sleep(3);
            return 0;
        }

        if(  answer[1] == 3 && 
            (memcmp(answer + 2, "YES", 3) == 0 || memcmp(answer + 2, "yes", 3) == 0) ) {    // correct answer for continuing? good
            break;
        }
    }

    (void) Cconws("\r\n");

    //-------------
    // if continuing, write boot sector and everything needed for partitioning
    PcSectorsPosition pcSectPos = {0, 8, 136, 264};
    res = writeBootAndOtherSectors_likeWin7(partParams, pcSectPos);

    if(!res) {
        (void) Cconws("\r\nPress any key to terminate...\r\n");

        Cnecin();
        return 0;
    }

    //-------------
    // show message that we're done and we need to reset the ST to apply new settings
    //            |                                        |
    (void) Cconws("\r\n");
    (void) Cconws("SD card was paritioned.                 \r\n");
    (void) Cconws("SD NOOB driver was activated.           \r\n");
    (void) Cconws("Reset your ST to access your card...    \r\n");

    Cnecin();
    return 0;
}

//--------------------------------------------
void getTosVersion(void)
{
    BYTE  *pSysBase     = (BYTE *) 0x000004F2;
    BYTE  *ppSysBase    = (BYTE *)  ((DWORD )  *pSysBase);          // get pointer to TOS address

    tosVersion          = (WORD  ) *(( WORD *) (ppSysBase + 2));    // TOS +2: TOS version
}

//--------------------------------------------
void getCE_API(void)
{
    // get address of cookie jar
    DWORD *cookieJarAddr    = (DWORD *) 0x05A0;
    DWORD *cookieJar        = (DWORD *) *cookieJarAddr;

    hdIf = NULL;

    if(cookieJar == 0) {                        // no cookie jar? it's an old ST and CE_DD wasn't loaded 
        return;
    }

    DWORD cookieKey, cookieValue;

    while(1) {                                  // go through the list of cookies
        cookieKey   = *cookieJar++;
        cookieValue = *cookieJar++;

        if(cookieKey == 0) {                    // end of cookie list? then cookie not found
            return;
        }

        if(cookieKey == 0x43455049) {           // is it 'CEPI' key? found it, store it, quit
            hdIf = (THDif *) cookieValue;
            return;
        }
    }
}
//--------------------------------------------
WORD requestSense(BYTE deviceId)
{
    BYTE cmd[CMD_LENGTH_SHORT];

    memset(cmd, 0, 6);
    cmd[0] = (deviceId << 5) | SCSI_C_REQUEST_SENSE;
    cmd[4] = 16;                                    // how many bytes should be sent

    hdIfCmdAsUser(1, cmd, 6, pDmaBuffer, 1);

    if(!hdIf->success || hdIf->statusByte != 0) {   // if command failed, or status byte is not OK, fail
        return 0xffff;
    }

    WORD val;
    val = (pDmaBuffer[2] << 8) | pDmaBuffer[12];    // WORD: senseKey is up, senseCode is down
    return val;
}
//--------------------------------------------
BYTE readWriteSector(BYTE deviceId, BYTE readNotWrite, DWORD sectorNumber)
{
    BYTE cmd[CMD_LENGTH_SHORT];

    memset(cmd, 0, 6);

    if(readNotWrite) {      // read
        cmd[0] = (deviceId << 5) | SCSI_C_READ6;
    } else {                // write
        cmd[0] = (deviceId << 5) | SCSI_C_WRITE6;
    }

    cmd[1] = (sectorNumber >> 16) & 0x1f;               // 5 bits only (1 GB limit)
    cmd[2] = (sectorNumber >>  8);
    cmd[3] = (sectorNumber      );

    cmd[4] = 1;                                         // 1 sector at the time

    hdIfCmdAsUser(readNotWrite, cmd, CMD_LENGTH_SHORT, pDmaBuffer, 1);

    if(!hdIf->success) {                                // if failed to do the command, fail
        return FALSE;
    }

    if(hdIf->statusByte == 0) {                         // cmd succeeded, status is zero? good!
        return TRUE;
    }

    // if came here, then was able to do the command, but the status byte wasn't OK - maybe media changed, so let's request sense
    WORD sense = requestSense(deviceId);

    if(sense == 0x0628) {                               // medium changed / inserted, try again
        hdIfCmdAsUser(readNotWrite, cmd, CMD_LENGTH_SHORT, pDmaBuffer, 1);

        if(hdIf->success && hdIf->statusByte == 0) {    // if success, good
            return TRUE;
        }
    }

    return FALSE;                                       // if sense is not medium_changed, or failed to get sense, or failed to do the R/W operation, fail
}

//--------------------------------------------
WORD calculateChecksum(WORD *pBfr)
{
    WORD sum = 0;
    int  i;

    for(i=0; i<256; i++) {  // sum WORDs of bootsector
        sum += *pBfr;
        pBfr++;
    }

    return sum;
}
//--------------------------------------------
void storeIntel24b(BYTE *p, DWORD value)
{
    p[0] = (BYTE) (value      );
    p[1] = (BYTE) (value >>  8);
    p[2] = (BYTE) (value >> 16);
}
//--------------------------------------------
void storeIntelDword(BYTE *p, DWORD value)
{
    p[0] = (BYTE) (value      );
    p[1] = (BYTE) (value >>  8);
    p[2] = (BYTE) (value >> 16);
    p[3] = (BYTE) (value >> 24);
}
//--------------------------------------------
void storeIntelWord(BYTE *p, WORD value)
{
    p[0] = (BYTE) (value      );
    p[1] = (BYTE) (value >>  8);
}
//--------------------------------------------
BYTE writeMBR(TBpb *partParams)
{
    BYTE res;

    //---------------------------
    // generate boot sector (MBR)
    memset(pDmaBuffer, 0, 512);

    memcpy(pDmaBuffer + 3, "SDNOO", 5); // this is our signature, it will tell CE_DD to enable SD NOOB over this SD card

    ////////////////////////
    // START: PC partition entry #1 - starting at 0x1be
    // CHS of first absolute sector in partition
    memcpy(pDmaBuffer + 0x1bf, "\x01\x01\x00\x06", 4);  // head 1, sector 1, cylinder 0, partition type: FAT16B

    // CHS of last absolute sector in partition
//  pDmaBuffer[0x1c3]
//  pDmaBuffer[0x1c4]
//  pDmaBuffer[0x1c5]

    // LBA of first absolute sector in partition
    storeIntelDword(pDmaBuffer + 0x1c6, partParams->pHiddenSectors);

    // number of sectors in partition
    storeIntelDword(pDmaBuffer + 0x1ca, partParams->maxSizeSectors + 1);

    // END: PC partition entry #1 
    ////////////////////////
    // START: Atari Partition Header #2  - starting at 0x1d2
    pDmaBuffer[0x1de] = 0x01;               // p_flg  - not bootable, does exist
    memcpy(pDmaBuffer + 0x1df, "BGM", 3);   // p_id   - BGM - bit partition

    DWORD *p_st     = (DWORD *) (pDmaBuffer + 0x1e2);
    DWORD *p_size   = (DWORD *) (pDmaBuffer + 0x1e6);

    *p_st   = partParams->pHiddenSectors + 1;   // p_st   - starting at sector - right after the PC starting sector
    *p_size = partParams->maxSizeSectors;       // p_size - store partition size in sectors
    // END: Atari Partition Header #2
    ////////////////////////

    pDmaBuffer[0x1fe] = 0x55;
    pDmaBuffer[0x1ff] = 0xaa;

    res = readWriteSector(SDcard.id, ACSI_WRITE, 0);

    if(!res) {
        (void) Cconws("Failed to write MBR!\r\n");
        return FALSE;
    }

    return TRUE;
}
//--------------------------------------------
BYTE writePcPartitionSector0(TBpb *partParams, DWORD toSectorNumber)
{
    BYTE res;

    memset(pDmaBuffer, 0, 512);

    // BPB Fields for FAT16 Volumes (according to MS TechNet)
    memcpy         (pDmaBuffer + 0x000, "\xEB\x3C\x90MSDOS5.0\x00\x02", 13);    // jump instruction, OEM ID, bytes per sector

    pDmaBuffer[0x0d] = partParams->pSectorsPerCluster;                  // sectors per cluster
    storeIntelWord (pDmaBuffer + 0x0e, partParams->pReservedSectors);   // reserved sectors

    memcpy         (pDmaBuffer + 0x10, "\x02\x00\x02\x00\x00\xf8", 6);  // number of FATs, root entries, small sectors, media descriptor

    storeIntelWord (pDmaBuffer + 0x16, partParams->pSectorsPerFat);     // sectors per FAT
    storeIntelWord (pDmaBuffer + 0x18, partParams->pSectorsPerTrack);   // sectors per track
    storeIntelWord (pDmaBuffer + 0x1a, partParams->pNumberOfHeads);     // number of heads
    storeIntelDword(pDmaBuffer + 0x1c, partParams->pHiddenSectors);     // hidden sectors
    storeIntelDword(pDmaBuffer + 0x20, partParams->maxSizeSectors);     // Large Sectors

    // Extended BPB Fields for FAT16 Volumes
    memcpy         (pDmaBuffer + 0x24, "\x80\x00\x29", 3);              // Physical Drive Number, Reserved, Extended boot signature

    storeIntelDword(pDmaBuffer + 0x27, serialNumber);                   // serial number

    memcpy         (pDmaBuffer + 0x2b, "SD NOOB    ", 11);              // Volume Label. A field once used to store the volume label.
    memcpy         (pDmaBuffer + 0x36, "FAT16   ",     8);              // File System Type, LONGLONG

    memcpy         (pDmaBuffer + 0x1fe, "\x55\xAA", 2);                 // End of Sector Marker

    res = readWriteSector(SDcard.id, ACSI_WRITE, toSectorNumber);

    if(!res) {
        (void) Cconws("Failed to write PC part sector 0!\r\n");
        return FALSE;
    }

    return TRUE;
}
//--------------------------------------------
BYTE writeAtariPartitionSector0(TBpb *partParams)
{
    BYTE res;

    memset(pDmaBuffer, 0, 512);

    DWORD realSectorsInAtariSector  = partParams->aBytesPerSector / 512;    // how much real (512 B) sectors there are in single Atari sector
    DWORD atariSectorsCount         = partParams->maxSizeSectors  / realSectorsInAtariSector;   // convert real sector count into Atari sector count

    // boot sector according to Atari Compendium
    memcpy         (pDmaBuffer + 0x03, "SDNOO", 5);                         // OEM    - SD NOOB
    storeIntel24b  (pDmaBuffer + 0x08, serialNumber);                       // serial number - just lower 24 bits 
    storeIntelWord (pDmaBuffer + 0x0b, partParams->aBytesPerSector);        // BPS    - bytes per sector
    pDmaBuffer     [             0x0d] = partParams->aSectorsPerCluster;    // SPC    - sectors per cluster
    storeIntelWord (pDmaBuffer + 0x0e, 0x0001);                             // RES    - reserved sectors
    pDmaBuffer     [             0x10] = 0x02;                              // NFATS  - number of FATs
    storeIntelWord (pDmaBuffer + 0x11, 0x0200);                             // NDIRS  - number of root dir entries
    storeIntelWord (pDmaBuffer + 0x13, atariSectorsCount);                  // NSECTS - number of sectors on disk
    pDmaBuffer     [             0x15] = 0xf8;                              // MEDIA descriptor
    storeIntelWord (pDmaBuffer + 0x16, partParams->aSectorsPerFat);         // SPF    - sectors per FAT
    storeIntelWord (pDmaBuffer + 0x18, partParams->pSectorsPerTrack);       // SPT    - sectors per track (same as on PC)
    storeIntelWord (pDmaBuffer + 0x1a, partParams->pNumberOfHeads);         // NSIDES - number of sides (heads)
    storeIntelWord (pDmaBuffer + 0x1c, partParams->pHiddenSectors + 1);     // NHID   - number of hidden sectors

    res = readWriteSector(SDcard.id, ACSI_WRITE, partParams->pHiddenSectors + 1);

    if(!res) {
        (void) Cconws("Failed to write Atari part sector 0!\r\n");
        return FALSE;
    }

    return TRUE;
}
//--------------------------------------------
BYTE writeFATtable(DWORD sectorNo)
{
    BYTE res;

    memset(pDmaBuffer, 0, 512);
    memcpy(pDmaBuffer, "\xF8\xFF\xFF\xFF", 4);

    res = readWriteSector(SDcard.id, ACSI_WRITE, sectorNo);

    if(!res) {
        (void) Cconws("Failed to write FAT table!\r\n");
        return FALSE;
    }

    return TRUE;
}
//--------------------------------------------
BYTE writeRootDirWithPartitionName(DWORD sectorNo)
{
    BYTE res;

    memset(pDmaBuffer     , 0, 512);
    memcpy(pDmaBuffer     , "SD NOOB    \x08", 12);                  // partition name and VOLUME LABEL flag
    memcpy(pDmaBuffer + 22, "\xEF\x5B\x63\x4A", 4);                  // some fake date and time

    res = readWriteSector(SDcard.id, ACSI_WRITE, sectorNo);

    if(!res) {
        (void) Cconws("Failed to write root dir entries.\r\n");
        return FALSE;
    }

    return TRUE;
}
//--------------------------------------------
BYTE writeBootAndOtherSectors_likeWin7(TBpb *partParams, PcSectorsPosition pcSectPos)
{
    BYTE res;

    res = writePcPartitionSector0(partParams, pcSectPos.bootsector);

    if(!res) {
        return FALSE;
    }

    res = writeFATtable(pcSectPos.fat1);

    if(!res) {
        return FALSE;
    }

    res = writeFATtable(pcSectPos.fat2);

    if(!res) {
        return FALSE;
    }

    res = writeRootDirWithPartitionName(pcSectPos.rootDirEntry);

    if(!res) {
        return FALSE;
    }

    return TRUE;
}
//--------------------------------------------
BYTE writeBootAndOtherSectors_likeHddriver(TBpb *partParams)
{
    BYTE res;

    res = writeMBR(partParams);

    if(!res) {
        return FALSE;
    }

    res = writePcPartitionSector0(partParams, partParams->pHiddenSectors);

    if(!res) {
        return FALSE;
    }

    res = writeAtariPartitionSector0(partParams);

    if(!res) {
        return FALSE;
    }

    return TRUE;
}
//--------------------------------------------
BYTE readBootsectorAndIdentifyContent(void)
{
    BYTE res;

    res = readWriteSector(SDcard.id, ACSI_READ, 0);

    if(!res) {
        (void) Cconws("Failed to read boot sector.\r\n");
        return FALSE;
    }

    (void) Cconws("Boot sector content   : ");

    WORD sum = calculateChecksum((WORD *) pDmaBuffer);

    if(sum == 0x1234) {                                                 // atari bootsector checksum ok?
        (void) Cconws("bootable code\r\n");
//  } else if(memcmp(pDmaBuffer + 3, "SDNOO", 5) == 0) {                // HDDRIVER like: SD noob signature?
    } else if(memcmp(pDmaBuffer + 0x2b, "SD NOOB", 7) == 0) {           // Win7     like: SD noob signature?
        (void) Cconws("SD NOOB part.\r\n");
    } else if(pDmaBuffer[510] == 0x55 && pDmaBuffer[511] == 0xaa) {     // PC boot signature?
        (void) Cconws("PC partition\r\n");
    } else {                                                            // there's something else in the boot sector
        (void) Cconws("something else\r\n");
    }

    return TRUE;
}
//--------------------------------------------

BYTE cs_inquiry(BYTE id)
{
    BYTE cmd[CMD_LENGTH_SHORT];
    
    memset(cmd, 0, 6);
    cmd[0] = (id << 5) | (SCSI_CMD_INQUIRY & 0x1f);
    cmd[4] = 32;                                                    // count of bytes we want from inquiry command to be returned
    
    hdIfCmdAsUser(ACSI_READ, cmd, CMD_LENGTH_SHORT, pDmaBuffer, 1); // issue the inquiry command and check the result 
    
    if(!hdIf->success || hdIf->statusByte != SCSI_STATUS_OK) {      // if failed, return FALSE 
        return FALSE;
    }

    return TRUE;
}
//--------------------------------------------
// scan the xCSI bus and detect all the devices that are there
void scanXCSIbus(void)
{
    BYTE i, res, isCE;

    (void) Cconws("Bus scan              : ");

    hdIf->maxRetriesCount = 0;                                  // disable retries - we are expecting that the devices won't answer on every ID

    for(i=0; i<8; i++) {
        res = cs_inquiry(i);                                    // try to read the INQUIRY string

        isCE            = FALSE;                                // it's not CE (at this moment)
        devicesOnBus[i] = DEVTYPE_NOTHING;                      // nothing here
        
        if(res) {                                               // something responded
            if(memcmp(pDmaBuffer + 16, "CosmosEx", 8) == 0) {   // inquiry string contains 'CosmosEx'
                isCE = TRUE;

                if(memcmp(pDmaBuffer + 27, "SD", 2) == 0) {     // it's CosmosEx SD card
                    devicesOnBus[i] = DEVTYPE_SD;
                } else {                                        // it's CosmosEx, but not SD card
                    devicesOnBus[i] = DEVTYPE_TRANSLATED;
                }
            } else if(memcmp(pDmaBuffer + 16, "CosmoSolo", 9) == 0) {   // it's CosmoSolo, that's SD card
                isCE = TRUE;

                devicesOnBus[i] = DEVTYPE_SD;
            } else {                                            // it's not CosmosEx and also not CosmoSolo
                devicesOnBus[i] = DEVTYPE_UNKNOWN;              // we don't know what it is, but it's there
            }        
        }

        if(isCE) {
            (void) Cconws("\33p");
            Cconout(i + '0');                                   // White-on-Black - it's CE
            (void) Cconws("\33q");
        } else {
            Cconout(i + '0');                                   // Black-on-White - not CE or not present
        }
    }

    (void) Cconws("\r\n");
    hdIf->maxRetriesCount = 10;                                 // enable retries
}

//--------------------------------------------------

BYTE findFirstIDofType(BYTE devType, int startIndex)
{
    int i;

    for(i=startIndex; i<8; i++) {           // go through IDs - ASCENDING
        if(devicesOnBus[i] == devType) {    // found it? good
            return i;
        }
    }

    return 0xff;                            // didn't find it? fail
}

//--------------------------------------------------
// go through the already detected devices, and return xCSI ID of the first which is CE 
BYTE getCEid(BYTE anyCEnotOnlySD)
{
    BYTE idCE = findFirstIDofType(DEVTYPE_TRANSLATED,   0);
    BYTE idSD = findFirstIDofType(DEVTYPE_SD,           0);

    if(anyCEnotOnlySD) {                    // if any CE ID will do, return the one which is not 0xff
        if(idSD != 0xff) {                  // got SD ID? return it
            return idSD;
        }

        return idCE;                        // if don't have SD ID, return CE ID (might end up with not having CE ID either)
    } else {                                // if looking only for SD ID, return it (might end up with not having SD ID)
        return idSD;
    }
}

//--------------------------------------------------
BYTE enableACSIidType(BYTE ceId, BYTE xCSIid, BYTE devType)
{
    // set xCSIid to devType in CE
    
    BYTE cmd[CMD_LENGTH_SHORT] = {0, 'C', 'E', HOSTMOD_TRANSLATED_DISK, TEST_SET_ACSI_ID, 0};

    cmd[0] = (ceId << 5);                                   // cmd[0] = CE ceId + TEST UNIT READY (0)   
    memset(pDmaBuffer, 0, 512);                             // clear the buffer 

    pDmaBuffer[0] = xCSIid;                                 // set this ID...
    pDmaBuffer[1] = devType;                                // ...to this device type
    
    hdIfCmdAsUser(ACSI_WRITE, cmd, CMD_LENGTH_SHORT, pDmaBuffer, 1); 

    if(!hdIf->success || hdIf->statusByte != 0) {           // if command failed...
        return FALSE;
    }

    return TRUE;
}
//--------------------------------------------------

// find an ID which could be used as new SD card ID
BYTE enableCEids(BYTE ceId)
{
    BYTE res;
 
    // talk to CE to get more precise info about assigned devices (but no SD card info)
    BYTE cmd[CMD_LENGTH_SHORT] = {0, 'C', 'E', HOSTMOD_TRANSLATED_DISK, TEST_GET_ACSI_IDS, 0};

    cmd[0] = (ceId << 5);                                   // cmd[0] = CE ceId + TEST UNIT READY (0)   
    memset(pDmaBuffer, 0, 512);                             // clear the buffer 

    hdIfCmdAsUser(ACSI_READ, cmd, CMD_LENGTH_SHORT, pDmaBuffer, 1); 

    if(!hdIf->success || hdIf->statusByte != 0) {           // if command failed...
        (void) Cconws("Failed to get info from CosmosEx\r\n");
        return FALSE;
    }

    // go through the received device types, and if it's SD, RAW or CE_DD, update the devicesOnBus[] array with these more precise info
    int i;
    for(i=0; i<8; i++) {
        if(pDmaBuffer[i] == DEVTYPE_SD || pDmaBuffer[i] == DEVTYPE_RAW || pDmaBuffer[i] == DEVTYPE_TRANSLATED) {        // this is what we wanted? if yes, store it
            devicesOnBus[i] = pDmaBuffer[i];
        }
    }

    //----------------
    // find the CE_DD device ID
    BYTE id_cedd = findFirstIDofType(DEVTYPE_TRANSLATED, 0); // find first CE_DD ID, which we would like to boot

    // if CE_DD is found, but it's on ID 7 (or 6, on TT SCSI), move it to lower ID, so we will have free slot for SD
    if(id_cedd >= 6) {
        BYTE emptyId;
        emptyId = findFirstIDofType(DEVTYPE_NOTHING, 0);    // find an empty slot

        if(emptyId == 0xff) {                               // couldn't find empty slot? fail!
            (void) Cconws("No free slot for reallocation of CE_DD!\r\n"); 
            return FALSE;
        }
        
        if(emptyId >= 6) {                                  // the new ID is still too high? fail
            (void) Cconws("Reallocation of CE_DD failed.\r\n"); 
            return FALSE;
        }

        res = enableACSIidType(ceId, emptyId, DEVTYPE_TRANSLATED);  // set new ID to CE_DD, that should also disable CE_DD on old ID

        if(!res) {                                          // if failed to set this dev type to ID, fail
            (void) Cconws("Failed to reallocate CE_DD on bus!\r\n");
            return FALSE;
        }
        
        devicesOnBus[id_cedd] = DEVTYPE_NOTHING;            // old slot: now nothing
        devicesOnBus[emptyId] = DEVTYPE_TRANSLATED;         // new slot: now CE_DD
        
        id_cedd = emptyId;                                  // this is the new CE_DD ID
    }

    // if CE_DD wasn't found, try to enable it
    if(id_cedd == 0xff) {                                   // if CE_DD is not enabled on bus, we need to enable it first
        BYTE emptyId;
        emptyId = findFirstIDofType(DEVTYPE_NOTHING, 0);    // find an empty slot

        if(emptyId == 0xff) {                               // couldn't find empty slot? fail!
            (void) Cconws("No free slot on bus for CE_DD!\r\n"); 
            return FALSE;
        }

        res = enableACSIidType(ceId, emptyId, DEVTYPE_TRANSLATED);  // enable CE_DD on this ID

        if(!res) {                                          // if failed to set this dev type to ID, fail
            (void) Cconws("Failed to enable CE_DD on bus!\r\n");
            return FALSE;
        }

        id_cedd = emptyId;                                  // this is our CE_DD now
        devicesOnBus[id_cedd] = DEVTYPE_TRANSLATED;         // store it in this field, too
    }

    //----------------
    // find the SD ID
    BYTE id_sd = findFirstIDofType(DEVTYPE_SD, id_cedd);    // find first SD ID, which we would like to access, and let it be after CE_DD ID (we want that to boot)

    // id_sd now might be:
    // number 1 to 7 -- OK
    // number 0xff   -- if SD isn't on bus at all
    // number 0xff   -- if SD is on the bus, but it's on lower ID than id_cedd, which is not good for our situation - we need CE_DD first, then SD

    if(id_sd == 0xff) {                                     // if SD is not enabled on bus (or is before CE_DD), we need to enable it (move it)
        BYTE emptyId;
        emptyId = findFirstIDofType(DEVTYPE_NOTHING, id_cedd);  // find an empty slot after CE_DD ID (we want that to boot)

        if(emptyId == 0xff) {                               // couldn't find empty slot? fail!
            (void) Cconws("No free slot on bus for SD!\r\n");
            return FALSE;
        }

        res = enableACSIidType(ceId, emptyId, DEVTYPE_SD);  // enable SD on this ID

        if(!res) {                                          // if failed to set this dev type to ID, fail
            (void) Cconws("Failed to enable SD on bus!\r\n");
            return FALSE;
        }

        id_sd = emptyId;                                    // this is our SD now
        devicesOnBus[id_sd] = DEVTYPE_SD;                   // store it in this field, too
    }

    SDcard.id = id_sd;                                      // store the valid ID to the struct
    return TRUE;
}

//--------------------------------------------------

BYTE getSDcardInfo(BYTE deviceID)
{
    // talk to CS to get more precise info about SD card (but not precise info about assigned device types)
    commandLong[0] = (deviceID << 5) | 0x1f;    // SD card device ID
    commandLong[3] = 'S';                       // for CS
    commandLong[5] = TEST_GET_ACSI_IDS;
    commandLong[6] = 0;                         // don't reset SD error counters

    hdIfCmdAsUser(ACSI_READ, commandLong, CMD_LENGTH_LONG, pDmaBuffer, 1);

    if(!hdIf->success || hdIf->statusByte != 0) {   // if command failed...
        return FALSE;
    }

    SDcard.id       = pDmaBuffer[9];                // ID
    SDcard.isInit   = pDmaBuffer[10];               // is init
    
    SDcard.SCapacity  = pDmaBuffer[15];             // get SD card capacity
    SDcard.SCapacity  = SDcard.SCapacity << 8;
    SDcard.SCapacity |= pDmaBuffer[16];
    SDcard.SCapacity  = SDcard.SCapacity << 8;
    SDcard.SCapacity |= pDmaBuffer[17];
    SDcard.SCapacity  = SDcard.SCapacity << 8;
    SDcard.SCapacity |= pDmaBuffer[18];
    
    return TRUE;
}

//--------------------------------------------------
void showSDcardCapacity(void)
{
    DWORD capacityMB = SDcard.SCapacity >> 11;      // sectors into MegaBytes

    if(capacityMB < 1024) {                         // less than 1 GB? show on 4 digits
        int length = 4;

        if(capacityMB <= 999) {                     // if it's less than 1000, show on 3 digits
            length = 3;
        }

        if(capacityMB <= 99) {                      // if it's less than 100, show on 2 digits
            length = 2;
        }

        if(capacityMB <= 9) {                       // if it's less than 10, show on 1 digits
            length = 1;
        }

        showInt(capacityMB, length);
        (void) Cconws(" MB\r\n");
    } else {                                        // more than 1 GB?
        int capacityGB      = capacityMB / 1024;    // GB part
        int capacityRest    = capacityMB % 1024;    // MB part

        capacityRest = capacityRest / 100;          // get only 100s of MB part 

        int length = (capacityGB <= 9) ? 1 : 2;     // if it's bellow 10, show 1 digit, otherwise show 2 digits

        showInt(capacityGB, length);                // show GB part
        (void) Cconws(".");
        showInt(capacityRest, 1);                   // show 100s of MB part

        (void) Cconws(" GB\r\n");
    }
}
//--------------------------------------------------
// global variables, later used for calling hdIfCmdAsSuper
BYTE __readNotWrite, __cmdLength;
WORD __sectorCount;
BYTE *__cmd, *__buffer;

void hdIfCmdAsSuper(void)
{
    // this should be called through Supexec()
    (*hdIf->cmd)(__readNotWrite, __cmd, __cmdLength, __buffer, __sectorCount);
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
//--------------------------------------------------
void showHexWord(WORD val)
{
    showHexByte((BYTE) (val >>  8));
    showHexByte((BYTE)  val);
}
//--------------------------------------------------
