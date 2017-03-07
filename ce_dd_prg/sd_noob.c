#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>

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
#include "serial.h"

// ------------------------------------------------------------------
void showInt(int value, int length);
void msleepInSuper(int ms);

extern BYTE FastRAMBuffer[];
extern WORD tosVersion;

//--------------------------------------------------

TSDcard          SDcard;
TSDnoobPartition SDnoobPartition;
_BPB             SDbpb;

//--------------------------------------------------
DWORD getIntelDword(BYTE *p)
{
    DWORD val;

    val  = p[3];
    val  = val << 8;
    val |= p[2];
    val  = val << 8;
    val |= p[1];
    val  = val << 8;
    val |= p[0];

    return val;
}
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
WORD requestSense(BYTE deviceId)
{
    BYTE cmd[CMD_LENGTH_SHORT];

    if(SERIALDEBUG) { aux_sendString("requestSense "); }
    
    memset(cmd, 0, 6);
    cmd[0] = (deviceId << 5) | SCSI_C_REQUEST_SENSE;
    cmd[4] = 16;                                    // how many bytes should be sent

    (*hdIf.cmd)(1, cmd, 6, pDmaBuffer, 1);

    if(!hdIf.success || hdIf.statusByte != 0) {     // if command failed, or status byte is not OK, fail
        if(SERIALDEBUG) { aux_sendString("FAILED\n"); }
        return 0xffff;
    }

    WORD val;
    val = (pDmaBuffer[2] << 8) | pDmaBuffer[12];    // WORD: senseKey is up, senseCode is down
    
    if(SERIALDEBUG) { aux_hexWord(val); aux_sendString("\n"); }
    return val;
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
int readWriteSector(BYTE deviceId, BYTE readNotWrite, DWORD sectorNumber, BYTE sectorCount, BYTE *pBuffer)
{
    BYTE cmd[CMD_LENGTH_SHORT];

    if(readNotWrite) {                              // read
        cmd[0] = (deviceId << 5) | SCSI_C_READ6;
    } else {                                        // write
        cmd[0] = (deviceId << 5) | SCSI_C_WRITE6;
    }

    if(SERIALDEBUG) { aux_sendString("readWriteSector "); aux_hexNibble(deviceId);          aux_sendChar(' '); 
                                                          aux_hexNibble(readNotWrite);      aux_sendChar(' '); 
                                                          aux_hexDword(sectorNumber);       aux_sendChar(' '); 
                                                          aux_hexByte (sectorCount);        aux_sendChar(' '); 
                                                          aux_hexDword((DWORD) pBuffer);    aux_sendChar(' '); }
    
    cmd[1] = (sectorNumber >> 16) & 0x1f;           // 5 bits only (1 GB limit)
    cmd[2] = (sectorNumber >>  8);
    cmd[3] = (sectorNumber      );

    cmd[4] = sectorCount;                           // sector count
    cmd[5] = 0;                                     // control = zero
    
    (*hdIf.cmd)(readNotWrite, cmd, CMD_LENGTH_SHORT, pBuffer, sectorCount);

    if(hdIf.success && hdIf.statusByte == OK) {     // if everything OK, success
        if(SERIALDEBUG) { aux_sendString(" E_OK\n"); }
        return E_OK;
    }

    if(!hdIf.success) {                             // command failed? drive not ready
        if(SERIALDEBUG) { aux_sendString(" EDRVNR\n"); }
        return EDRVNR;
    }

    //---------
    // if came here, cmd succeeded, but returned something other than OK - let's request sense
    WORD sense = requestSense(deviceId);            // request sense

    if(sense == 0xffff || sense == 0x023A) {        // request sense failed or media not present? drive not ready
        if(SERIALDEBUG) { aux_sendString(" EDRVNR\n"); }
        return EDRVNR;
    }

    if(sense == 0x0628) {                           // media changed? return media changed
        if(SERIALDEBUG) { aux_sendString(" E_CHNG\n"); }
        return E_CHNG;
    }

    //--------
    // if came here, couldn't identify any specific sense, so just return generic R/W error
    if(SERIALDEBUG) { aux_sendString(" EREADF / EWRITF\n"); }
    return (readNotWrite ? EREADF : EWRITF);
}

//--------------------------------------------------
BYTE gotSDnoobCard(void)
{
    BYTE res;

    SDcard.mediaChanged     = FALSE;                // we're (re)reading this, no media change 

    SDnoobPartition.enabled = FALSE;                // SD NOOB not enabled (yet)
    SDnoobPartition.driveNo = 2;                    // now fixed as drive 'C' 

    //-------------
    // get the SD card ID, info, size, is init
    res = getSDcardIDonBus();                       // find SD card on bus

    if(!res) {                                      // not found? quit
        if(SERIALDEBUG) { aux_sendString("gotSDnoobCard - no SD on bus\n"); }
        
        if(SDnoobPartition.verboseInit) {
            (void) Cconws("SD NOOB: no SD on bus, skipping\r\n");
        }

        return FALSE;
    }

    res = getSDcardInfo();                          // try to get SD card info

    if(!res) {                                      // failed to get info? quit
        if(SERIALDEBUG) { aux_sendString("gotSDnoobCard - getSDcardInfo() failed\n"); }
        
        if(SDnoobPartition.verboseInit) {
            (void) Cconws("SD NOOB: failed to get SD info\r\n");
        }

        return FALSE;
    }

    if(!SDcard.isInit) {                            // if card not initialized, quit
        if(SERIALDEBUG) { aux_sendString("gotSDnoobCard - !SDcard.isInit\n"); }
        
        if(SDnoobPartition.verboseInit) {
            (void) Cconws("SD NOOB: card not present or not init\r\n");
        }

        return FALSE;
    }

    if(SDcard.SCapacity < 0x64000) {                // card smaller than 200 MB? don't use it
        if(SERIALDEBUG) { aux_sendString("gotSDnoobCard - SCapacity too small\n"); }
    
        if(SDnoobPartition.verboseInit) {
            (void) Cconws("SD NOOB: card too small, skipping\r\n");
        }

        return FALSE;
    }

    //-------------
    // get MBR, find out if it's SD NOOB
    int ires = readWriteSector(SDcard.id, ACSI_READ, 0, 1, pDmaBuffer); // read MBR

    if(ires == E_CHNG) {        // if first read ended with media change, try again, the 2nd read will probably succeed
        ires = readWriteSector(SDcard.id, ACSI_READ, 0, 1, pDmaBuffer); // read MBR
    }

    if(ires != E_OK) {          // failed to get info? quit
        if(SERIALDEBUG) { aux_sendString("gotSDnoobCard - readWriteSector(MBR) failed\n"); }
    
        if(SDnoobPartition.verboseInit) {
            (void) Cconws("SD NOOB: failed to read MBR\r\n");
        }

        return FALSE;
    }

    //if(memcmp(pDmaBuffer + 3, "SDNOO", 5) != 0) {         // HDDRIVER like: if it doesn't have SD NOOB marker, quit
    if(memcmp(pDmaBuffer + 0x2b, "SD NOOB", 7) != 0) {      // Win7     like: if it doesn't have SD NOOB marker, quit
        if(SERIALDEBUG) { aux_sendString("gotSDnoobCard - not SD NOOB\n"); }

        if(SDnoobPartition.verboseInit) {
            (void) Cconws("SD NOOB: card not SD NOOB, skipping\r\n");
        }

        return FALSE;
    }

    SDnoobPartition.sectorStart = 0;                                    // starting sector of partition - where the partition has boot sector 
    SDnoobPartition.sectorCount = getIntelDword(pDmaBuffer + 0x20);     // large sectors    - partition size in sectors

    if(SERIALDEBUG) { 
        aux_sendString("gotSDnoobCard() - part sector start: ");    aux_hexWord (SDnoobPartition.sectorStart);
        aux_sendString(", count: ");                                aux_hexDword(SDnoobPartition.sectorCount);
        aux_sendString("\n");
    }

    //-----------------------
    // fill the BPB structure for usage
    // note: AS stands for Atari Sectors (which are larger than normal 512 sectors)
    WORD sectorsPerFat      = getIntelWord (pDmaBuffer + 0x16);                 // how many 512 B sectors will FAT have (0x40 or 0x80)
    WORD sectorsPerAS       = (tosVersion <= 0x102) ? 16 : 8;                   // how many 512 B sectors will be in Atari sector? TOS 1.02 will have 16 s (8 kB), newer will have 8 s (4 kB)
    WORD bytesPerAS         = sectorsPerAS * 512;                               // TOS 1.02 and less? 8kBPS, otherwise 4kBPS
    WORD rootDirSizeAS      = (512 * 32) / bytesPerAS;                          // size of ROOT directory, in Atari sectors = (count_of_root_dir_entries * 32B) / bytes_per_atari_sector

    SDbpb.recsiz            = bytesPerAS;                                       // bytes per atari sector (8 kB / 4 kB)
    SDbpb.clsiz             = 2;                                                // sectors per cluster 
    SDbpb.clsizb            = SDbpb.clsiz * SDbpb.recsiz;                       // bytes per cluster = sectors per cluster * bytes per sector
    SDbpb.rdlen             = rootDirSizeAS;                                    // sector length of root directory
    SDbpb.fsiz              = sectorsPerFat / sectorsPerAS;                     // sectors per FAT
    SDbpb.fatrec            = 1 + SDbpb.fsiz;                                   // starting sector of second FAT (boot sector on 0, then FAT1 of size fsiz)
    SDbpb.datrec            = SDbpb.fatrec + SDbpb.fsiz + SDbpb.rdlen;          // start of data = start_of_FAT2 + size_of_FAT + size_of_root_dir_in_sectors

    WORD atariSectorsCount  = SDnoobPartition.sectorCount / sectorsPerAS;       // how many Atari sectors we have (which have more than 1 real 512 B sectors)
    SDbpb.numcl             = (atariSectorsCount - SDbpb.datrec) / SDbpb.clsiz; // number of clusters = (count_of_atari_sectors - sector_where_the_clusters_start) / sectors_per_cluster

    SDbpb.bflags            = 1;                                                // bit 0=1 - 16 bit FAT, else 12 bit

    if(SERIALDEBUG) { 
        aux_sendString("gotSDnoobCard - BPB:");
        aux_sendString("\n  recsiz: "); aux_hexWord(SDbpb.recsiz);
        aux_sendString("\n  clsiz : "); aux_hexWord(SDbpb.clsiz);
        aux_sendString("\n  clsizb: "); aux_hexWord(SDbpb.clsizb);
        aux_sendString("\n  rdlen : "); aux_hexWord(SDbpb.rdlen);
        aux_sendString("\n  fsiz  : "); aux_hexWord(SDbpb.fsiz);
        aux_sendString("\n  fatrec: "); aux_hexWord(SDbpb.fatrec);
        aux_sendString("\n  datrec: "); aux_hexWord(SDbpb.datrec);
        aux_sendString("\n  numcl : "); aux_hexWord(SDbpb.numcl);
        aux_sendString("\n  bflags: "); aux_hexWord(SDbpb.bflags);
        aux_sendString("\n");
        aux_sendString("\n  drive : "); aux_sendChar(SDnoobPartition.driveNo + 'A');
        aux_sendString("\n  size  : "); aux_hexWord (SDnoobPartition.sectorCount >> 11);
        aux_sendString(" MB\n\n");
    }
    //--------------------
    // we're ready to use the SD card
    if(SDnoobPartition.verboseInit) {
        (void) Cconws("SD NOOB: drive \33p");
        Cconout(SDnoobPartition.driveNo + 'A');
        (void) Cconws("\33q, size: ");

        DWORD megaBytes = SDnoobPartition.sectorCount >> 11;        // sectors into MegaBytes
        showCapacity(megaBytes);
        (void) Cconws("\r\n");
    }

    SDnoobPartition.physicalPerAtariSector = sectorsPerAS;          // how many physical sectors fit into single Atari sector (8, 16, 32)

    SDnoobPartition.enabled = TRUE;                                 // SD NOOB is enabled
    return TRUE;
}
//--------------------------------------------------
DWORD SDnoobRwabs(WORD mode, BYTE *pBuffer, WORD logicalSectorCount, WORD logicalStartingSector, WORD device)
{
    BYTE readNotWrite   = (mode & (1 << 0)) == 0;       // 0 = Read, 1 = Write
    BYTE noRetries      = (mode & (1 << 2)) != 0;       // if non-zero, then no retries
    BYTE noTranslate    = (mode & (1 << 3)) != 0;       // if non-zero, physical mode (if zero, logical mode)

    if(SERIALDEBUG) { 
        aux_sendString("SDnoobRwabs r/w:"); aux_hexNibble(readNotWrite);
        aux_sendString(" noRet:");          aux_hexNibble(noRetries);
        aux_sendString(" noTra:");          aux_hexNibble(noTranslate);
        aux_sendString(" logStart:");       aux_hexWord(logicalStartingSector);
        aux_sendString(" logCount:");       aux_hexWord(logicalSectorCount);
    }

    if(noTranslate) {                                   // physical mode? bad request
        if(SERIALDEBUG) { aux_sendString(" EBADRQ\n"); }
        return EBADRQ;
    }

    BYTE  toFastRam     =  (((DWORD) pBuffer) >= 0x1000000) ? TRUE  : FALSE;        // flag: are we reading to FAST RAM?
    BYTE  bufAddrIsOdd  = ((((DWORD) pBuffer) & 1) == 0)    ? FALSE : TRUE;         // flag: buffer pointer is on ODD address?
    BYTE  useMidBuffer  = (toFastRam || bufAddrIsOdd);                              // flag: is load to fast ram or on odd address, use middle buffer

    DWORD maxSectorCount = useMidBuffer ? (FASTRAM_BUFFER_SIZE / 512) : MAXSECTORS; // how many sectors we can read at once - if going through middle buffer then middle buffer size, otherwise max sector coun
    int   ires;

    WORD i, triesCount;
    triesCount = noRetries ? 1 : 3;     // how many times we should try?

    if(SERIALDEBUG) { 
        aux_sendString(" midBuf:");         aux_hexNibble(useMidBuffer);

        aux_sendString("\npart start: ");   aux_hexDword(SDnoobPartition.sectorStart);
        aux_sendString(", ps/as: ");        aux_hexDword(SDnoobPartition.physicalPerAtariSector);
        aux_sendChar('\n');
    }
    
    // physical starting sector = the starting sector of partition + physical sector of where we want to start reading
    DWORD physicalStartingSector    = SDnoobPartition.sectorStart + (logicalStartingSector * SDnoobPartition.physicalPerAtariSector);
    DWORD physicalSectorCount       = logicalSectorCount * SDnoobPartition.physicalPerAtariSector;

    if(SERIALDEBUG) { 
        aux_sendString("phy start : ");     aux_hexDword(physicalStartingSector);
        aux_sendString(", count: ");        aux_hexDword(physicalSectorCount);
        aux_sendChar('\n');
    }

    while(physicalSectorCount > 0) {
        DWORD thisSectorCount   = (physicalSectorCount < maxSectorCount) ? physicalSectorCount : maxSectorCount;    // will the needed read size be within the blockSize, or not?
        DWORD thisByteCount     = thisSectorCount << 9;

        for(i=0; i<triesCount; i++) {
            if(useMidBuffer) {                              // through middle buffer?
                ires = readWriteSector(SDcard.id, readNotWrite, physicalStartingSector, thisSectorCount, FastRAMBuffer);

                if(ires == E_OK) {                          // if succeeded, copy it to right place
                    memcpy(pBuffer, FastRAMBuffer, thisByteCount);
                }
            } else {                                        // directly to final buffer?
                ires = readWriteSector(SDcard.id, readNotWrite, physicalStartingSector, thisSectorCount, pBuffer);
            }

            if(ires == E_OK) {                              // if succeeded, break out of retries loop
                break;
            }

            if(ires == E_CHNG) {                            // if media changed, don't try anymore, tell TOS about this and he will hopefully re-read stuff
                SDcard.mediaChanged = TRUE;
                if(SERIALDEBUG) { aux_sendString("SDnoobRwabs - E_CHNG\n"); }
                return E_CHNG;
            }
        }

        if(ires != E_OK) {                                  // if failed, quit, and just return what the R/W function returned
            if(SERIALDEBUG) { aux_sendString("SDnoobRwabs - !E_OK\n"); }
            return ires;
        }

        physicalSectorCount     -= thisSectorCount;         // now we need to read less sectors
        physicalStartingSector  += thisSectorCount;         // advance to next sectors

        pBuffer                 += thisByteCount;           // advance in the buffer
    }

    if(SERIALDEBUG) { aux_sendString("SDnoobRwabs - E_OK\n"); }
    return E_OK;                                            // if came here, everything is fine
}
//--------------------------------------------------
