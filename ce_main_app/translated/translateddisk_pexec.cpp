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
#include "translateddisk.h"
#include "gemdos.h"
#include "gemdos_errno.h"
#include "desktopcreator.h"

extern THwConfig hwConfig;
extern InterProcessEvents events;

/* Pexec() related sub commands:
    - create image for Pexec() - WRITE
    - GetBpb()                 - READ
    - read sector              - READ (WORD sectorStart, WORD sectorCount)
*/

void TranslatedDisk::onPexec(BYTE *cmd)
{
    switch(cmd[5]) {
        case PEXEC_CREATE_IMAGE:    onPexec_createImage(cmd);   return;
        case PEXEC_GET_BPB:         onPexec_getBpb(cmd);        return;
        case PEXEC_READ_SECTOR:     onPexec_readSector(cmd);    return;
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
	
    // create file and close it
    FILE *f = fopen(hostName.c_str(), "rb");                        // open according to required mode

    if(!f) {
        Debug::out(LOG_DEBUG, "TranslatedDisk::onPexec_createImage() - %s - fopen failed!", (char *) hostName.c_str());

        dataTrans->setStatus(EACCDN);                               // if failed to create, access error
        return;
    }
    
    fseek(f, 0, SEEK_END);                                          // move to the end of file
    int fileSizeBytes = ftell(f);                                   // store the position of the end of file
    fseek(f, 0, SEEK_SET);                                          // set to start of file again

    if(fileSizeBytes > PEXEC_DRIVE_USABLE_SIZE_BYTES) {
        fclose(f);
        Debug::out(LOG_DEBUG, "TranslatedDisk::onPexec_createImage() - the file is too big to fit in our image!");

        dataTrans->setStatus(EINTRN);                               // if file too big, error
        return;
    }
    
    createImage(fullAtariPath, f, fileSizeBytes);                   // now create the image    
    
    fclose(f);                                                      // close the file
    dataTrans->setStatus(E_OK);                                     // ok!
}

void TranslatedDisk::createImage(std::string &fullAtariPath, FILE *f, int fileSizeBytes)
{
    memset(pexecImage, 0, PEXEC_DRIVE_SIZE_BYTES);                  // clear the whole image
    memset(pexecImageReadFlags, 0, PEXEC_DRIVE_SIZE_SECTORS);       // clear all the READ flags

    prgSectorStart  = 0;
    prgSectorEnd    = PEXEC_DRIVE_SIZE_SECTORS;
    
    DWORD fat1startingSector = 4;
    DWORD fat2startingSector = fat1startingSector + PEXEC_FAT_SECTORS_NEEDED;
    
    DWORD dataSectorAbsolute = fat2startingSector + PEXEC_FAT_SECTORS_NEEDED;   // absolute sector - from the start of the image (something like sector #84)
    DWORD dataSectorRelative = 2;                                               // relative sector - relative sector numbering from the start of data area (probably #2)
    
    //--------------
    // split path to dirs
    #define MAX_DIR_NESTING     64
    std::string strings[MAX_DIR_NESTING];
    int start = 0, pos;
    int i, found = 0;

    // first split the string by separator
    while(1) {
        pos = fullAtariPath.find(HOSTPATH_SEPAR_CHAR, start);

        if(pos == -1) {                                 // not found?
            strings[found] = fullAtariPath.substr(start);   // copy in the rest
            found++;
            break;
        }

        strings[found] = fullAtariPath.substr(start, (pos - start));
        found++;

        start = pos + 1;

        if(found >= MAX_DIR_NESTING) {              // sanitize possible overflow
            break;
        }
    }    
    
    if(found < 1) {                                 // couldn't break it to string? fail
        dataTrans->setStatus(EINTRN);
        return;    
    }
    
    //--------------
    // first add dirs to image as dir entries
    BYTE *pFat1 = pexecImage + (fat1startingSector * 512);  // pointer to FAT1
    BYTE *pFat2 = pexecImage + (fat2startingSector * 512);  // pointer to FAT2

    int curSectorAbs = dataSectorAbsolute;                  // start at root dir
    int curSectorRel = dataSectorRelative;                  
    
    WORD date = 0, time = 0;
    
    for(i=0; i<(found-1); i++) {
        createDirEntry(i == 0, true, date, time, 0,             (char *) strings[i].c_str(),            curSectorAbs, curSectorRel);   // store DIR entry 
        storeFatChain (pFat1, curSectorRel, curSectorRel);  // mark that DIR entry in FAT chain
        curSectorAbs++;
        curSectorRel++;   
    }
    
    // then add the file to image as dir entry
    createDirEntry(found < 2, false, date, time, fileSizeBytes, (char *) strings[found - 1].c_str(),    curSectorAbs, curSectorRel);
    storeFatChain (pFat1, curSectorRel, curSectorRel);      // mark that DIR entry in FAT chain
    curSectorAbs++;
    curSectorRel++;   
    
    // now add the file content to the image
    BYTE *pFileInImage = pexecImage + (curSectorAbs * 512); // get pointer to file in image
    fread(pFileInImage, 1, fileSizeBytes, f);
    
    int fileSizeSectors = (fileSizeBytes / 512) + ((fileSizeBytes % 512) == 0) ? 0 : 1; // calculate how many sector the file takes
    
    prgSectorStart  = curSectorRel;                         // first sector - where the PRG starts
    prgSectorEnd    = curSectorRel + fileSizeSectors - 1;   // last sector  - where the PRG ends
    
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

    for(int i=sectorStart; i<sectorEnd; i++) {  // go through all the sectors
        pwFat[i] = i+1;                         // current sector points to next sector
    }

    pwFat[sectorEnd] = 0xffff;                  // ending sector = last sector in chain marker
}

void TranslatedDisk::createDirEntry(bool isRoot, bool isDir, WORD date, WORD time, DWORD fileSize, char *dirEntryName, int sectorNoAbs, int sectorNoRel)
{
    BYTE  *pSector     = pexecImage + (sectorNoAbs * 512);  // get pointer to this sector
    DWORD nextSectorNo = sectorNoRel + 1;
    
    memcpy(pSector + 0, dirEntryName, 11);          // filename
    pSector[11] = isDir ? 0x10 : 0x00;              // DIR / File flag
    
    storeIntelWord (pSector + 22, time);            // time
    storeIntelWord (pSector + 24, date);            // date
    storeIntelWord (pSector + 26, nextSectorNo);    // starting cluster (sector), little endian
    storeIntelDword(pSector + 28, fileSize);        // file size, little endian
}

void TranslatedDisk::onPexec_getBpb(BYTE *cmd)
{
    #define ROOTDIR_SIZE            1
    #define FAT1_STARTING_SECTOR    4
    
    dataTrans->addDataWord(512);                        // bytes per sector
    dataTrans->addDataWord(1);                          // sectors per cluster
    dataTrans->addDataWord(512);                        // bytes per cluster
    dataTrans->addDataWord(ROOTDIR_SIZE);               // sector length of root directory
    dataTrans->addDataWord(PEXEC_FAT_SECTORS_NEEDED);   // sectors per FAT
    dataTrans->addDataWord(FAT1_STARTING_SECTOR + PEXEC_FAT_SECTORS_NEEDED);                        // starting sector of second FAT
    dataTrans->addDataWord(FAT1_STARTING_SECTOR + (2 * PEXEC_FAT_SECTORS_NEEDED) + ROOTDIR_SIZE);   // starting sector of data
    dataTrans->addDataWord(PEXEC_DRIVE_SIZE_SECTORS);   // clusters per disk
    dataTrans->addDataWord(1);                          // bit 0=1 - 16 bit FAT, else 12 bit

    dataTrans->padDataToMul16();
    dataTrans->setStatus(E_OK);
}

void TranslatedDisk::onPexec_readSector(BYTE *cmd)
{
    WORD  startingSector    = Utils::getWord(cmd + 6);
    WORD  sectorCount       = Utils::getWord(cmd + 8);
    DWORD byteCount         = sectorCount * 512;

    if(startingSector + sectorCount > PEXEC_DRIVE_SIZE_SECTORS) {       // would be out of boundary? fail
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
   
