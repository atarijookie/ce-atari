#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <unistd.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ce_dd_prg.h"
#include "xbra.h"
#include "acsi.h"
#include "translated.h"
#include "gemdos.h"
#include "gemdos_errno.h"
#include "bios.h"
#include "main.h"
#include "hdd_if.h"
#include "sd_noob.h"

// ------------------------------------------------------------------
void showInt(int value, int length);

extern BYTE FastRAMBuffer[];

//--------------------------------------------------

TSDcard          SDcard;
TSDnoobPartition SDnoobPartition;
_BPB             SDbpb;

//--------------------------------------------------
WORD getIntelWord(BYTE *p)
{
    WORD val;

    val  = p[1];
    val  = val << 8;
    val |= p[0];

    return val;
}

//--------------------------------------------------
void showCapacity(DWORD megaBytes)
{
    int length = 4;

    if(megaBytes <= 999) {                     // if it's less than 1000, show on 3 digits
        length = 3;
    }

    if(megaBytes <= 99) {                      // if it's less than 100, show on 2 digits
        length = 2;
    }

    if(megaBytes <= 9) {                       // if it's less than 10, show on 1 digits
        length = 1;
    }

    showInt(megaBytes, length);
    (void) Cconws(" MB");
} 
//--------------------------------------------------
BYTE getSDcardIDonBus(void)
{
    SDcard.id = 0xff;                               // no ID yet

    // talk to CE to get more precise info about assigned devices (but no SD card info)
    commandShort[4] = TEST_GET_ACSI_IDS;
    commandShort[5] = 0;

    memset(pDmaBuffer, 0, 512);

    (*hdIf.cmd)(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);

    if(!hdIf.success || hdIf.statusByte != 0) {   // if command failed...
        return FALSE;
    }

    // go through the received device types, and if it's SD, store it and quit
    int i;
    for(i=0; i<8; i++) {
        if(pDmaBuffer[i] == DEVTYPE_SD) {           // found it? quit
            SDcard.id = i;
            return TRUE;
        }
    }

    return FALSE;                                   // if came here, didn't find SD card on bus
}

//--------------------------------------------------
BYTE getSDcardInfo(void)
{
    // init, in case we fail
    SDcard.isInit       = FALSE;
    SDcard.SCapacity    = 0;

    // talk to CS to get more precise info about SD card (but not precise info about assigned device types)
    commandLong[3] = 'S';                           // command type: for CS
    commandLong[5] = TEST_GET_ACSI_IDS;
    commandLong[6] = 0;                             // don't reset SD error counters

    (*hdIf.cmd)(ACSI_READ, commandLong, CMD_LENGTH_LONG, pDmaBuffer, 1);

    commandLong[3] = 'E';                           // restore command type: for CE

    if(!hdIf.success || hdIf.statusByte != 0) {   // if command failed...
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
BYTE readWriteSector(BYTE deviceId, BYTE readNotWrite, DWORD sectorNumber, BYTE sectorCount, BYTE *pBuffer)
{
    BYTE cmd[CMD_LENGTH_SHORT];

    memset(cmd, 0, 6);

    if(readNotWrite) {                              // read
        cmd[0] = (deviceId << 5) | SCSI_C_READ6;
    } else {                                        // write
        cmd[0] = (deviceId << 5) | SCSI_C_WRITE6;
    }

    cmd[1] = (sectorNumber >> 16) & 0x1f;           // 5 bits only (1 GB limit)
    cmd[2] = (sectorNumber >>  8);
    cmd[3] = (sectorNumber      );

    cmd[4] = sectorCount;                           // sector count

    (*hdIf.cmd)(readNotWrite, cmd, CMD_LENGTH_SHORT, pBuffer, 1);

    if(!hdIf.success || hdIf.statusByte != OK) {    // if failed, return FALSE 
        return FALSE;
    }

    return TRUE;
}

//--------------------------------------------------
BYTE gotSDnoobCard(void)
{
    BYTE res;

    SDnoobPartition.enabled = FALSE;                // SD NOOB not enabled (yet)
    SDnoobPartition.driveNo = 2;                    // now fixed as drive 'C' 

    //-------------
    // get the SD card ID, info, size, is init
    res = getSDcardIDonBus();                       // find SD card on bus

    if(!res) {                                      // not found? quit
        (void) Cconws("SD NOOB: no SD on bus, skipping\r\n");
        return FALSE;
    }

    res = getSDcardInfo();                          // try to get SD card info

    if(!res) {                                      // failed to get info? quit
        (void) Cconws("SD NOOB: failed to get SD info\r\n");
        return FALSE;
    }

    if(!SDcard.isInit) {                            // if card not initialized, quit
        (void) Cconws("SD NOOB: card not present or not init\r\n");
        return FALSE;
    }

    if(SDcard.SCapacity < 0x64000) {                // card smaller than 200 MB? don't use it
        (void) Cconws("SD NOOB: card too small, skipping\r\n");
        return FALSE;
    }

    //-------------
    // get MBR, find out if it's SD NOOB
    res = readWriteSector(SDcard.id, ACSI_READ, 0, 1, pDmaBuffer);  // read MBR

    if(!res) {                                      // failed to get info? quit
        (void) Cconws("SD NOOB: failed to read MBR\r\n");
        return FALSE;
    }

    if(memcmp(pDmaBuffer + 3, "SDNOO", 5) != 0) {   // if it doesn't have SD NOOB marker, quit
        (void) Cconws("SD NOOB: card not SD NOOB, skipping\r\n");
        return FALSE;
    }

    DWORD *p_st     = (DWORD *) (pDmaBuffer + 0x1e2);
    DWORD *p_size   = (DWORD *) (pDmaBuffer + 0x1e6);

    SDnoobPartition.sectorStart = *p_st;            // p_st   - starting at sector - right after the PC starting sector
    SDnoobPartition.sectorCount = *p_size;          // p_size - store partition size in sectors

    res = readWriteSector(SDcard.id, ACSI_READ, SDnoobPartition.sectorStart, 1, pDmaBuffer);  // read Atari boot sector

    if(!res) {                                      // failed to get info? quit
        (void) Cconws("SD NOOB: failed to read boot sector\r\n");
        return FALSE;
    }

    //-----------------------
    // fill the BPB structure for usage 
    SDbpb.recsiz            = getIntelWord(pDmaBuffer + 0x0b);      // bytes per atari sector
    SDbpb.clsiz             = pDmaBuffer  [             0x0d];      // sectors per cluster 
    SDbpb.clsizb            = SDbpb.clsiz * SDbpb.recsiz;           // bytes per cluster = sectors per cluster * bytes per sector
    SDbpb.rdlen             = 2;                                    // sector length of root directory 
    SDbpb.fsiz              = getIntelWord(pDmaBuffer + 0x16);      // sectors per FAT 
    SDbpb.fatrec            = 1 + SDbpb.fsiz;                       // starting sector of second FAT (boot sector on 0, then FAT1 of size fsiz)

    WORD ndirs              = getIntelWord(pDmaBuffer + 0x11);      // NDIRS - number of ROOT directory entries
    WORD rootDirSizeInAtariSectors = (ndirs * 32) / SDbpb.recsiz;   // size of ROOT directory, in Atari sectors = (count_of_root_dir_entries * 32B) / bytes_per_atari_sector

    SDbpb.datrec            = SDbpb.fatrec + SDbpb.fsiz + rootDirSizeInAtariSectors;    // start of data = start_of_FAT2 + size_of_FAT + size_of_root_dir_in_sectors

    WORD atariSectorsCount  = getIntelWord(pDmaBuffer + 0x13);                          // how many Atari sectors we have (which have more than 1 real 512 B sectors)
    SDbpb.numcl             = (atariSectorsCount - SDbpb.datrec) / SDbpb.clsiz;         // number of clusters = (count_of_atari_sectors - sector_where_the_clusters_start) / sectors_per_cluster

    SDbpb.bflags            = 1;                                    // bit 0=1 - 16 bit FAT, else 12 bit

    //--------------------
    // we're ready to use the SD card
    (void) Cconws("SD NOOB: drive \33p");
    Cconout(SDnoobPartition.driveNo + 'A');
    (void) Cconws("\33q, size: ");

    DWORD megaBytes = SDnoobPartition.sectorCount >> 11;            // sectors into MegaBytes
    showCapacity(megaBytes);
    (void) Cconws("\r\n");

    SDnoobPartition.physicalPerAtariSector = SDbpb.recsiz / 512;    // how many physical sectors fit into single Atari sector (8, 16, 32)

    SDnoobPartition.enabled = TRUE;                                 // SD NOOB is enabled
    return TRUE;
}
//--------------------------------------------------
DWORD SDnoobRwabs(WORD mode, BYTE *pBuffer, WORD logicalSectorCount, WORD logicalStartingSector, WORD device)
{
    BYTE readNotWrite   = (mode & (1 << 0)) == 0;       // 0 = Read, 1 = Write
    BYTE noRetries      = (mode & (1 << 2)) != 0;       // if non-zero, then no retries
    BYTE noTranslate    = (mode & (1 << 3)) != 0;       // if non-zero, physical mode (if zero, logical mode)

    if(noTranslate) {                                   // physical mode? bad request
        return -5;
    }

    BYTE  toFastRam     =  (((DWORD) pBuffer) >= 0x1000000) ? TRUE  : FALSE;        // flag: are we reading to FAST RAM?
    BYTE  bufAddrIsOdd  = ((((DWORD) pBuffer) & 1) == 0)    ? FALSE : TRUE;         // flag: buffer pointer is on ODD address?
    BYTE  useMidBuffer  = (toFastRam || bufAddrIsOdd);                              // flag: is load to fast ram or on odd address, use middle buffer

    DWORD maxSectorCount = useMidBuffer ? (FASTRAM_BUFFER_SIZE / 512) : MAXSECTORS; // how many sectors we can read at once - if going through middle buffer then middle buffer size, otherwise max sector coun
    BYTE res;

    WORD i, triesCount;
    triesCount = noRetries ? 1 : 3;     // how many times we should try?

    // physical starting sector = the starting sector of partition + physical sector of where we want to start reading
    DWORD physicalStartingSector    = SDnoobPartition.sectorStart + (logicalStartingSector * SDnoobPartition.physicalPerAtariSector);
    DWORD physicalSectorCount       = logicalSectorCount * SDnoobPartition.physicalPerAtariSector;

    while(physicalSectorCount > 0) {
        DWORD thisSectorCount   = (physicalSectorCount < maxSectorCount) ? physicalSectorCount : maxSectorCount;    // will the needed read size be within the blockSize, or not?
        DWORD thisByteCount     = thisSectorCount << 9;

        for(i=0; i<triesCount; i++) {
            if(useMidBuffer) {                              // through middle buffer?
                res = readWriteSector(SDcard.id, readNotWrite, physicalStartingSector, thisSectorCount, FastRAMBuffer);

                if(res) {                                   // if succeeded, copy it to right place
                    memcpy(pBuffer, FastRAMBuffer, thisByteCount);
                }
            } else {                                        // directly to final buffer?
                res = readWriteSector(SDcard.id, readNotWrite, physicalStartingSector, thisSectorCount, pBuffer);
            }

            if(res) {                                       // if succeeded, break out of retries loop
                break;
            }
        }

        if(!res) {                                          // if failed, fail and quit
            return -11;                                     // Read fault
        }

        physicalSectorCount     -= thisSectorCount;         // now we need to read less sectors
        physicalStartingSector  += thisSectorCount;         // advance to next sectors

        pBuffer                 += thisByteCount;           // advance in the buffer
    }

    return 0;
}
//--------------------------------------------------
