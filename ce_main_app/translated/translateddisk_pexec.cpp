#include <string>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>

#include "../global.h"
#include "../debug.h"
#include "../settings.h"
#include "../utils.h"
#include "../mounter.h"
#include "acsidatatrans.h"
#include "translateddisk.h"
#include "translatedhelper.h"
#include "filenameshortener.h"
#include "gemdos.h"
#include "gemdos_errno.h"
#include "desktopcreator.h"

extern THwConfig hwConfig;
extern InterProcessEvents events;

// if PEXEC_FULL_PATH, then RAW IMAGE will contain full path -> \FULL\PATH\TO\MY.PRG will be created
//#define PEXEC_FULL_PATH    
    
/* Pexec() related sub commands:
    - create image for Pexec()          - WRITE
    - GetBpb()                          - READ
    - read sector                       - READ  (WORD sectorStart, WORD sectorCount)
    - write sector (for debugging only) - WRITE (WORD sectorStart, WORD sectorCount)
*/

void TranslatedDisk::onPexec(BYTE *cmd)
{
    switch(cmd[5]) {
        case PEXEC_CREATE_IMAGE:    onPexec_createImage(cmd);   return;
        case PEXEC_GET_BPB:         onPexec_getBpb(cmd);        return;
        case PEXEC_READ_SECTOR:     onPexec_readSector(cmd);    return;
        case PEXEC_WRITE_SECTOR:    onPexec_writeSector(cmd);   return;
    }
    
    // if came here, bad sub command
    dataTrans->setStatus(E_NOTHANDLED);
}

void TranslatedDisk::onPexec_createImage(BYTE *cmd)
{
    bool res = dataTrans->recvData(dataBuffer, 512);    // get data from Hans

    if(!res) {
        Debug::out(LOG_DEBUG, "TranslatedDisk::onPexec_createImage() - failed to receive data...");
        dataTrans->setStatus(EINTRN);
        return;
    }

    WORD mode = Utils::getWord(dataBuffer);

    Debug::out(LOG_DEBUG, "TranslatedDisk::onPexec_createImage() - mode: %04x, path: %s", mode, (char *) dataBuffer + 2);

    std::string atariName, hostName;

    convertAtariASCIItoPc((char *) (dataBuffer + 2));               // try to fix the path with only allowed chars
    atariName =           (char *) (dataBuffer + 2);                // get file name

    bool        waitingForMount;
    int         atariDriveIndex, zipDirNestingLevel;
    std::string fullAtariPath;
    res = createFullAtariPathAndFullHostPath(atariName, fullAtariPath, atariDriveIndex, hostName, waitingForMount, zipDirNestingLevel);

    Debug::out(LOG_DEBUG, "TranslatedDisk::onPexec_createImage() - will fake raw drive %c:", 'A' + atariDriveIndex);
    pexecDriveIndex = atariDriveIndex;                              // we will fake this drive index as RAW drive for Pexec() usage
    
    if(pexecDriveIndex >= 0 && pexecDriveIndex < MAX_DRIVES) {      // if index valid, drive changed
        conf[pexecDriveIndex].mediaChanged = true;
    } else {
        Debug::out(LOG_DEBUG, "TranslatedDisk::onPexec_createImage() - drive %c: seems to be invalid!", 'A' + pexecDriveIndex);
    }
    
    if(!res) {                                                      // the path doesn't bellong to us?
        Debug::out(LOG_DEBUG, "TranslatedDisk::onPexec_createImage() - %s - createFullAtariPath failed", (char *) atariName.c_str());

		dataTrans->setStatus(E_NOTHANDLED);                         // if we don't have this, not handled
        return;
    }

    if(waitingForMount) {                                           // if the path will be available in a while, but we're waiting for mount to finish now
        Debug::out(LOG_DEBUG, "TranslatedDisk::onPexec_createImage() -- waiting for mount, call this function again to succeed later.");
    
        dataTrans->setStatus(E_WAITING_FOR_MOUNT);
        return;
    }
    
    if(!hostPathExists(hostName)) {			                        // and the file does not exist, quit with FILE NOT FOUND
        Debug::out(LOG_DEBUG, "TranslatedDisk::onPexec_createImage() - %s - fopen mode is just read, but the file does not exist, failed!", (char *) hostName.c_str());

        dataTrans->setStatus(EFILNF);
        return;
    }
	
    //------------
    // get path to PRG and store it
    Utils::splitFilenameFromPath(fullAtariPath, pexecPrgPath, pexecPrgFilename);
    
    if(pexecPrgPath.length() > 1) {
        int  lastCharIndex  = pexecPrgPath.length() - 1;
        
        if(pexecPrgPath[lastCharIndex] == HOSTPATH_SEPAR_CHAR) {    // if the string terminated with path separator, remove it
            pexecPrgPath.erase(lastCharIndex);
        }
    }
    pathSeparatorHostToAtari(pexecPrgPath);                         // change '/' to '\'
    
    pexecFakeRootPath    = "X:\\";                                  // create fake short path as if the PRG was always in the root of the drive
    pexecFakeRootPath   += pexecPrgFilename;
    pexecFakeRootPath[0] = 'A' + pexecDriveIndex;
    
    Debug::out(LOG_DEBUG, "TranslatedDisk::onPexec_createImage() - pexecPrgPath: %s, pexecFakeRootPath: %s", (char *) pexecPrgPath.c_str(), (char *) pexecFakeRootPath.c_str());

    //------------
    // create file and close it
    FILE *f = fopen(hostName.c_str(), "rb");                        // open according to required mode

    if(!f) {
        Debug::out(LOG_DEBUG, "TranslatedDisk::onPexec_createImage() - %s - fopen failed!", (char *) hostName.c_str());

        dataTrans->setStatus(EACCDN);                               // if failed to create, access error
        return;
    }
    
    //----------
    // get file date & time
    struct stat attr;
    tm *timestr;

	int ires = stat(hostName.c_str(), &attr);					    // get the file status
	
	if(ires != 0) {
		Debug::out(LOG_ERROR, "TranslatedDisk::onPexec_createImage -- stat() failed, errno %d", errno);
	}

	timestr = localtime(&attr.st_mtime);			    	        // convert time_t to tm structure

    WORD atariTime = Utils::fileTimeToAtariTime(timestr);
    WORD atariDate = Utils::fileTimeToAtariDate(timestr);
    //----------

    fseek(f, 0, SEEK_END);                                          // move to the end of file
    int fileSizeBytes = ftell(f);                                   // store the position of the end of file
    fseek(f, 0, SEEK_SET);                                          // set to start of file again

    if(fileSizeBytes > PEXEC_DRIVE_USABLE_SIZE_BYTES) {
        fclose(f);
        Debug::out(LOG_DEBUG, "TranslatedDisk::onPexec_createImage() - the file is too big to fit in our image!");

        dataTrans->setStatus(EINTRN);                               // if file too big, error
        return;
    }
    
    createImage(fullAtariPath, f, fileSizeBytes, atariTime, atariDate); // now create the image    
    
    fclose(f);                                                      // close the file
    dataTrans->setStatus(E_OK);                                     // ok!
}

void TranslatedDisk::createImage(std::string &fullAtariPath, FILE *f, int fileSizeBytes, WORD atariTime, WORD atariDate)
{
    int fileSizeSectors = (fileSizeBytes / 512) + (((fileSizeBytes % 512) == 0) ? 0 : 1); // calculate how many sector the file takes

    Debug::out(LOG_DEBUG, "TranslatedDisk::createImage() - fullAtariPath: %s, fileSizeBytes: %d, fileSizeSectors: %d", (char *) fullAtariPath.c_str(), fileSizeBytes, fileSizeSectors);

    prgSectorStart  = 0;
    prgSectorEnd    = PEXEC_DRIVE_SIZE_SECTORS;
    
    DWORD fat1startingSector = 4;
    DWORD fat2startingSector = fat1startingSector + PEXEC_FAT_SECTORS_NEEDED;
    
    DWORD dataSectorAbsolute = fat2startingSector + PEXEC_FAT_SECTORS_NEEDED;   // absolute sector - from the start of the image (something like sector #84)
    DWORD dataSectorRelative = 1;                                               // relative sector - relative sector numbering from the start of data area (probably #2)

    //memset(pexecImage,        0, PEXEC_DRIVE_SIZE_BYTES);                                     // clear the whole image
    memset(pexecImage,          0, (fat1startingSector + 2*PEXEC_FAT_SECTORS_NEEDED) * 512);    // clear just the FAT section of image (to speed this up)
    
    memset(pexecImageReadFlags, 0, PEXEC_DRIVE_SIZE_SECTORS);                   // clear all the READ flags
    
    //--------------
    BYTE *pFat1 = pexecImage + (fat1startingSector * 512);  // pointer to FAT1
    BYTE *pFat2 = pexecImage + (fat2startingSector * 512);  // pointer to FAT2

    int curSectorAbs = dataSectorAbsolute;                  // start at root dir
    int curSectorRel = dataSectorRelative;                  
    
    bool isRootDir = true;    
    //--------------
    #define MAX_DIR_NESTING     64
    std::string strings[MAX_DIR_NESTING];
    int found = 0;
    
#ifdef PEXEC_FULL_PATH    
    // split path to dirs
    int start = 0, pos;
    int i;

    // first split the string by separator
    while(1) {
        pos = fullAtariPath.find(HOSTPATH_SEPAR_CHAR, start);

        if(pos == -1) {                                 // not found?
            strings[found] = fullAtariPath.substr(start);   // copy in the rest
            found++;
            break;
        }

        strings[found] = fullAtariPath.substr(start, (pos - start));
        
        if(strings[found].length() > 0) {           // valid string?
            Debug::out(LOG_DEBUG, "TranslatedDisk::createImage() - valid string: %s", (char *) strings[found].c_str());
            found++;
        } else {
            Debug::out(LOG_DEBUG, "TranslatedDisk::createImage() - invalid string skipped");
        }

        start = pos + 1;

        if(found >= MAX_DIR_NESTING) {              // sanitize possible overflow
            break;
        }
    }    
    
    if(found < 1) {                                 // couldn't break it to string? fail
        Debug::out(LOG_DEBUG, "TranslatedDisk::createImage() - could not explode fullAtariPath: %s", (char *) fullAtariPath.c_str());
        dataTrans->setStatus(EINTRN);
        return;    
    }

    Debug::out(LOG_DEBUG, "TranslatedDisk::createImage() - fullAtariPath: %s exploded into %d pieces", (char *) fullAtariPath.c_str(), found);
    
    //--------------
    // first add dirs to image as dir entries
    for(i=0; i<(found-1); i++) {
        Debug::out(LOG_DEBUG, "TranslatedDisk::createImage() - LOOP storing DIR ENTRY: %s on curSectorAbs: %d, curSectorRel: %d", (char *) strings[i].c_str(), curSectorAbs, curSectorRel);

        createDirEntry(isRootDir, true, atariDate, atariTime, 0, (char *) strings[i].c_str(), curSectorAbs, curSectorRel);   // store DIR entry 

        if(!isRootDir) {    // not adding to root dir? mark FAT chain
            storeFatChain (pFat1, curSectorRel, curSectorRel);  // mark that DIR entry in FAT chain
        } else {            // adding to root dir? don't mark FAT chain
            isRootDir = false;
        }

        curSectorAbs++;
        curSectorRel++;   
    }
#endif
    //------------
    // then add the file to image as dir entry
#ifdef PEXEC_FULL_PATH
    Debug::out(LOG_DEBUG, "TranslatedDisk::createImage() - LAST storing DIR ENTRY: %s on curSectorAbs: %d, curSectorRel: %d", (char *) strings[found - 1].c_str(), curSectorAbs, curSectorRel);
#else
    found = 1;
    strings[0] = pexecPrgFilename;
    
    Debug::out(LOG_DEBUG, "TranslatedDisk::createImage() - storing PRG as DIR ENTRY to root: %s on curSectorAbs: %d, curSectorRel: %d", (char *) strings[0].c_str(), curSectorAbs, curSectorRel);
#endif

    createDirEntry(isRootDir, false, atariDate, atariTime, fileSizeBytes, (char *) strings[found - 1].c_str(), curSectorAbs, curSectorRel);

    if(!isRootDir) {    // not adding to root dir? mark FAT chain
        storeFatChain (pFat1, curSectorRel, curSectorRel);      // mark that DIR entry in FAT chain
    } else {            // adding to root dir? don't mark FAT chain
        isRootDir = false;
    }

    curSectorAbs++;
    curSectorRel++;   
    
    //------------
    // now add the file content to the image
    prgSectorStart  = curSectorRel;                         // first sector - where the PRG starts
    prgSectorEnd    = curSectorRel + fileSizeSectors - 1;   // last sector  - where the PRG ends
    Debug::out(LOG_DEBUG, "TranslatedDisk::createImage() - storing PRG on relative sectors %d - %d (absolute starting sector: %d)", prgSectorStart, prgSectorEnd, curSectorAbs);

    BYTE *pFileInImage = pexecImage + (curSectorAbs * 512); // get pointer to file in image
    fread(pFileInImage, 1, fileSizeBytes, f);
    
    storeFatChain (pFat1, prgSectorStart, prgSectorEnd);    // mark PRG location in FAT chain
    //-------------
    //and now make copy of FAT1 to FAT2
    memcpy(pFat2, pFat1, PEXEC_FAT_BYTES_NEEDED);  
}

void TranslatedDisk::storeFatChain(BYTE *pbFat, WORD sectorStart, WORD sectorEnd)
{
    WORD *pwFat = (WORD *) pbFat;

    sectorStart = (sectorStart  < PEXEC_DRIVE_SIZE_SECTORS) ? sectorStart   : (PEXEC_DRIVE_SIZE_SECTORS - 1);
    sectorEnd   = (sectorEnd    < PEXEC_DRIVE_SIZE_SECTORS) ? sectorEnd     : (PEXEC_DRIVE_SIZE_SECTORS - 1);

    Debug::out(LOG_DEBUG, "TranslatedDisk::storeFatChain() - storing chain %d .. %d", sectorStart, sectorEnd);

    for(int i=sectorStart; i<sectorEnd; i++) {  // go through all the sectors
        pwFat[i] = i+1;                         // current sector points to next sector
    }

    pwFat[sectorEnd] = 0xffff;                  // ending sector = last sector in chain marker
}

void TranslatedDisk::createDirEntry(bool isRoot, bool isDir, WORD date, WORD time, DWORD fileSize, char *dirEntryName, int sectorNoAbs, int sectorNoRel)
{
    BYTE *pSector = pexecImage + (sectorNoAbs * 512);   // get pointer to this sector
    
    if(!isRoot) {
        storeDirEntry(pSector +  0, (char *) ".          ", true, time, date, sectorNoRel,  0); // pointer to this dir

        int prevDirSector   = sectorNoRel - 1; 
        int upDirSector     = prevDirSector > 1 ? prevDirSector : 0;                            // If updir is not root dir, use that number; otherwise store 0 for root dir sector.
        storeDirEntry(pSector + 32, (char *) "..         ", true, time, date, upDirSector,  0); // pointer to up   dir
    }

    // now convert the short 'FILE.C' to 'FILE    .C  '
    char deNameExtended[14];
    FilenameShortener::extendWithSpaces(dirEntryName, deNameExtended);

    // and convert FILE    .C  ' to 'FILE       ' 
    char ext[3];
    memcpy(ext, deNameExtended + 9, 3);     // get extension
    memcpy(deNameExtended + 8, ext, 3);     // and remove dot

    int offset = isRoot ? 0 : 64;
    storeDirEntry(pSector + offset, deNameExtended, isDir, time, date, sectorNoRel + 1, fileSize);  // pointer to next dir entries or this file content
}

void TranslatedDisk::storeDirEntry(BYTE *pEntry, char *dirEntryName, bool isDir, WORD time, WORD date, WORD startingSector, DWORD entrySizeBytes)
{
    memcpy(pEntry + 0, dirEntryName, 11);          // filename
    pEntry[11] = isDir ? 0x10 : 0x00;              // DIR / File flag
    
    storeIntelWord (pEntry + 22, time);            // time
    storeIntelWord (pEntry + 24, date);            // date
    storeIntelWord (pEntry + 26, startingSector);  // starting cluster (sector), little endian
    storeIntelDword(pEntry + 28, entrySizeBytes);  // file size, little endian
}

void TranslatedDisk::onPexec_getBpb(BYTE *cmd)
{
    Debug::out(LOG_DEBUG, "TranslatedDisk::onPexec_getBpb() - PEXEC_FAT_SECTORS_NEEDED: %d, PEXEC_DRIVE_SIZE_SECTORS: %d", PEXEC_FAT_SECTORS_NEEDED, PEXEC_DRIVE_SIZE_SECTORS);

    #define ROOTDIR_SIZE            1
    #define FAT1_STARTING_SECTOR    4
    
    dataTrans->addDataWord(512);                                                                    //  0- 1: bytes per sector
    dataTrans->addDataWord(1);                                                                      //  2- 3: sectors per cluster
    dataTrans->addDataWord(512);                                                                    //  4- 5: bytes per cluster
    dataTrans->addDataWord(ROOTDIR_SIZE);                                                           //  6- 7: sector length of root directory
    dataTrans->addDataWord(PEXEC_FAT_SECTORS_NEEDED);                                               //  8- 9: sectors per FAT
    dataTrans->addDataWord(FAT1_STARTING_SECTOR + PEXEC_FAT_SECTORS_NEEDED);                        // 10-11: starting sector of second FAT
    dataTrans->addDataWord(FAT1_STARTING_SECTOR + (2 * PEXEC_FAT_SECTORS_NEEDED) + ROOTDIR_SIZE);   // 12-13: starting sector of data
    dataTrans->addDataWord(PEXEC_DRIVE_SIZE_SECTORS);                                               // 14-15: clusters per disk
    dataTrans->addDataWord(1);                                                                      // 16-17: bit 0=1 - 16 bit FAT, else 12 bit

    dataTrans->addDataByte(pexecDriveIndex);                                                        // 18   : index of drive, which will now be RAW Pexec() drive
    dataTrans->addZerosUntilSize(32);                                                               // 19-31: zeros
    
    const char *pPrgPath;
    
#ifdef PEXEC_FULL_PATH
    // if full path, store full path to PRG
    pPrgPath        = pexecPrgPath.c_str();
#else
    // if not full path, then store just path to root
    pPrgPath        = "\"";
#endif
    
    dataTrans->addDataCString(pPrgPath, false);                                      // 32 .. ??: path to PRG file

    dataTrans->addZerosUntilSize(256);                                                              // ??-255: zeros
    dataTrans->addDataCString(pexecPrgFilename.c_str(), false); // 256 .. ??: just the PRG filename (without path)
    
    dataTrans->addZerosUntilSize(384);                                                              // ??-383: zeros
    dataTrans->addDataCString(pexecFakeRootPath.c_str(), false); // 384 .. ??: just the PRG filename (without path)
    
    dataTrans->padDataToMul16();
    dataTrans->setStatus(E_OK);

    dataTrans->dumpDataOnce();
}

void TranslatedDisk::onPexec_readSector(BYTE *cmd)
{
    WORD  startingSector    = Utils::getWord(cmd + 6);
    WORD  sectorCount       = Utils::getWord(cmd + 8);
    DWORD byteCount         = sectorCount * 512;

    Debug::out(LOG_DEBUG, "TranslatedDisk::onPexec_readSector() - startingSector: %d, sectorCount: %d", startingSector, sectorCount);

    if(startingSector + sectorCount > PEXEC_DRIVE_SIZE_SECTORS) {       // would be out of boundary? fail
        Debug::out(LOG_DEBUG, "TranslatedDisk::onPexec_readSector() - out of range!");

        dataTrans->setStatus(EINTRN);
        return;
    }
    
    BYTE *pSector = pexecImage + (startingSector * 512);                // get pointer to start of the sector
    dataTrans->addDataBfr(pSector, byteCount, true);                    // add that data to dataTrans

    for(int i=0; i<sectorCount; i++) {                                  // now mark those read sectors as already read
        pexecImageReadFlags[startingSector + i] = 1; 
    }
    
    dataTrans->setStatus(E_OK);                                         // everything OK
}

void TranslatedDisk::onPexec_writeSector(BYTE *cmd)
{
    WORD  startingSector    = Utils::getWord(cmd + 6);
    WORD  sectorCount       = Utils::getWord(cmd + 8);
    DWORD byteCount         = sectorCount * 512;

    Debug::out(LOG_DEBUG, "TranslatedDisk::onPexec_writeSector() - startingSector: %d, sectorCount: %d", startingSector, sectorCount);

    if(startingSector + sectorCount > PEXEC_DRIVE_SIZE_SECTORS) {       // would be out of boundary? fail
        Debug::out(LOG_DEBUG, "TranslatedDisk::onPexec_writeSector() - out of range!");

        dataTrans->setStatus(EINTRN);
        return;
    }
    
    bool res = dataTrans->recvData(dataBuffer, byteCount);              // get data from Hans

    if(!res) {
        Debug::out(LOG_DEBUG, "TranslatedDisk::onPexec_writeSector() - failed to receive data...");
        dataTrans->setStatus(EINTRN);
        return;
    }
    
    BYTE *pSector = pexecImage + (startingSector * 512);                // get pointer to start of the sector
    memcpy(pSector, dataBuffer, byteCount);
    
    dataTrans->setStatus(E_OK);                                         // everything OK
}

bool TranslatedDisk::pexecWholeFileWasRead(void)
{
    int i;
    
    // go through all the sectors of the PRG and see if all were read
    for(i = prgSectorStart; i <= prgSectorEnd; i++) {
        if(pexecImageReadFlags[i] == 0) {       // this sector wasn't read? The whole wasn't read yet
            return false;
        }
    }
    
    return true;                                // we didn't find a sector which wasn't read, so the whole file was read
}

void TranslatedDisk::storeIntelWord(BYTE *p, WORD a)
{      
    p[0] = (BYTE) (a     );
    p[1] = (BYTE) (a >> 8);
}
   
void TranslatedDisk::storeIntelDword(BYTE *p, DWORD a)
{      
    p[0] = (BYTE) (a      );
    p[1] = (BYTE) (a >>  8);
    p[2] = (BYTE) (a >> 16);
    p[3] = (BYTE) (a >> 24);
}
   
