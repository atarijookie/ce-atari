#include <stdio.h>
#include <string.h>

#include <unistd.h>

#include <algorithm>

#include "../global.h"
#include "../debug.h"
#include "../utils.h"
#include "../periodicthread.h"
#include "../acsidatatrans.h"
#include "../translated/gemdos_errno.h"
#include "floppysetup.h"
#include "imagesilo.h"
#include "imagelist.h"
#include "imagestorage.h"
#include "floppysetup_commands.h"

volatile uint8_t currentImageDownloadStatus;
extern SharedObjects shared;

FloppySetup::FloppySetup()
{
    dataTrans   = NULL;
    up          = NULL;
    reloadProxy = NULL;

    currentUpload.fh = NULL;

    currentDownload.fh = NULL;

    screenResolution = 1;
}

FloppySetup::~FloppySetup()
{

}

void FloppySetup::setSettingsReloadProxy(SettingsReloadProxy *rp)
{
    reloadProxy = rp;
}

void FloppySetup::setAcsiDataTrans(AcsiDataTrans *dt)
{
    dataTrans = dt;
}

void FloppySetup::processCommand(uint8_t *command)
{
    cmd = command;

    if(dataTrans == 0) {
        Debug::out(LOG_ERROR, "FloppySetup::processCommand was called without valid dataTrans, can't tell ST that his went wrong...");
        return;
    }

    dataTrans->clear();                 // clean data transporter before handling

    logCmdName(cmd[4]);

    switch(cmd[4]) {
        case FDD_CMD_IDENTIFY:                          // return identification string
            dataTrans->addDataBfr("CosmosEx floppy setup", 21, true);       // add identity string with padding
            dataTrans->setStatus(FDD_OK);
            break;

        case FDD_CMD_GETSILOCONTENT:                    // return the filenames and contents of current floppy images in silo
            uint8_t bfr[512];
            shared.imageSilo->dumpStringsToBuffer(bfr);

            dataTrans->addDataBfr(bfr, 512, true);
            dataTrans->setStatus(FDD_OK);
            break;

        case FDD_CMD_UPLOADIMGBLOCK_START:      uploadStart();              break;

        case FDD_CMD_UPLOADIMGBLOCK_FULL:
        case FDD_CMD_UPLOADIMGBLOCK_PART:       uploadBlock();              break;

        case FDD_CMD_UPLOADIMGBLOCK_DONE_OK:
        case FDD_CMD_UPLOADIMGBLOCK_DONE_FAIL:  uploadEnd(false);           break;

        case FDD_CMD_SWAPSLOTS:                 shared.imageSilo->swap(cmd[5]);    dataTrans->setStatus(FDD_OK);	break;
        case FDD_CMD_REMOVESLOT:                shared.imageSilo->remove(cmd[5]);  dataTrans->setStatus(FDD_OK);	break;

        case FDD_CMD_NEW_EMPTYIMAGE:            newImage();                 break;
        case FDD_CMD_GET_CURRENT_SLOT:          getCurrentSlot();           break;
        case FDD_CMD_SET_CURRENT_SLOT:          setCurrentSlot();           break;
        case FDD_CMD_GET_IMAGE_ENCODING_RUNNING: getImageEncodingRunning(); break;

        case FDD_CMD_DOWNLOADIMG_START:         downloadStart();            break;
        case FDD_CMD_DOWNLOADIMG_ONDEVICE:      downloadOnDevice();         break;
        case FDD_CMD_DOWNLOADIMG_GETBLOCK:      downloadGetBlock();         break;
        case FDD_CMD_DOWNLOADIMG_DONE:          downloadDone();             break;

        case FDD_CMD_SEARCH_INIT:               searchInit();               break;
        case FDD_CMD_SEARCH_STRING:             searchString();             break;
        case FDD_CMD_SEARCH_RESULTS:            searchResult();             break;
        case FDD_CMD_SEARCH_REFRESHLIST:        searchRefreshList();        break;

        case FDD_CMD_SEARCH_DOWNLOAD2STORAGE:   break;
        case FDD_CMD_SEARCH_INSERT2SLOT:        searchInsertToSlot();       break;
        case FDD_CMD_SEARCH_DELETEFROMSTORAGE:  break;
    }

    dataTrans->sendDataAndStatus();         // send all the stuff after handling, if we got any
}

void FloppySetup::searchInit(void)
{
    screenResolution = cmd[5];              // current resolution on Atari ST

    if(!shared.imageList->exists()) {        // if the file does not yet exist, tell ST that we're downloading
        dataTrans->setStatus(FDD_DN_LIST);
        return;
    }

    if(!shared.imageList->getIsLoaded()) {  // if list is not loaded yet
        if(!shared.imageList->loadList()) { // try to load the list, if failed, error
            dataTrans->setStatus(FDD_ERROR);
            return;
        }
    }

    dataTrans->setStatus(FDD_OK);           // done
}

void FloppySetup::searchString(void)
{
    dataTrans->recvData(bfr64k, 512);       // get one sector from ST

    shared.imageList->search((char *) bfr64k);	// try to search for this string

    dataTrans->setStatus(FDD_OK);           // done
}

void FloppySetup::searchResult(void)
{
	int pageSize = cmd[5];              // page size
    int page = Utils::getWord(cmd + 6); // retrieve # of page (0 .. max page - 1)

    int pageStart   = page * pageSize;              // starting index of this page
    int pageEnd     = (page + 1) * pageSize;        // ending index of this page (actually start of new page)

    int results = shared.imageList->getSearchResultsCount();

    pageStart   = MIN(pageStart,    results);
    pageEnd     = MIN(pageEnd,      results);

    int realPage    = pageStart / pageSize;         // calculate the real page number
    int totalPages  = (results   / pageSize) + 1;   // calculate the count of pages we have

    dataTrans->addDataWord((uint16_t) realPage);        // byte 0, 1: real page
    dataTrans->addDataWord((uint16_t) totalPages);      // byte 2, 3: total pages

    memset(bfr64k, 0, 1024);

    // now get the search results - 68 bytes per line
    int offset = 0;
    for(int i=pageStart; i<pageEnd; i++) {
        shared.imageList->getResultByIndex(i, (char *) (bfr64k + offset), screenResolution);
        offset += 68;
    }

    dataTrans->addDataBfr(bfr64k, pageSize * 68, true);
    dataTrans->setStatus(FDD_OK);                   // done
}

void FloppySetup::searchInsertToSlot(void)
{
    dataTrans->recvData(bfr64k, 512);   // read data

    bool bres = shared.imageStorage->doWeHaveStorage();

    if(!bres) {         // don't have storage? fail
        dataTrans->setStatus(FDD_ERROR);
        return;
    }

	int pageSize = (int) bfr64k[0];			// 0   : items per page
    int page = Utils::getWord(bfr64k + 1);	// 1, 2: page number
    int rowNo = (int) bfr64k[3];			// 3   : row number on page
    int slotNo = (int) bfr64k[4];			// 4   : slot number

    if(slotNo < 0 || slotNo > 2) {         // slot out of range? fail
        dataTrans->setStatus(FDD_ERROR);
        return;
    }

    int itemIndex = (page * pageSize) + rowNo;

    std::string imageName;      // get image name by index of item
    bres = shared.imageList->getImageNameFromResultsByIndex(itemIndex, imageName);

    if(!bres) {         // couldn't get image name? fail
        dataTrans->setStatus(FDD_ERROR);
        return;
    }

    // check if we got this floppy image file, and if we don't - fail
    bres = shared.imageStorage->weHaveThisImage(imageName.c_str());

    if(!bres) {          // we don't have this image, quit
        dataTrans->setStatus(FDD_ERROR);
        return;
    }

    // get local path for this image
    std::string localImagePath;
    shared.imageStorage->getImageLocalPath(imageName.c_str(), localImagePath);

    // set floppy image to slot
    std::string sPath, sFile, sEmpty;
    Utils::splitFilenameFromPath(localImagePath, sPath, sFile);
    shared.imageSilo->add(slotNo, sFile, localImagePath, sEmpty, true);

    dataTrans->setStatus(FDD_OK);               // done
}

void FloppySetup::downloadOnDevice(void)
{
    TranslatedDisk * translated = TranslatedDisk::getInstance();
    int index = cmd[5];

    memset(bfr64k, 0, 512);
    dataTrans->recvData(bfr64k, 512);                       // receive file name into this buffer
    std::string atariFilePath = (char *) bfr64k;
    std::string hostPath;
    std::string pathWithHostSeparators;

    // try to convert atari path to host path (will work for translated drives, will fail for native)
    bool        waitingForMount, res;
    int         atariDriveIndex;
    std::string fullAtariPath;
    res = translated->createFullAtariPathAndFullHostPath(atariFilePath, fullAtariPath, atariDriveIndex, hostPath, waitingForMount);

    if(res) {                                               // good? file is on translated drive
        res = Utils::copyFile(currentDownload.fh, hostPath);

        if(res) {                                           // on device copy -- GOOD!
            Debug::out(LOG_DEBUG, "FloppySetup::searchDownloadOnDevice -- on device copy success!");

            fclose(currentDownload.fh);                     // close the previously opened file
            currentDownload.fh = NULL;

            if(index == 10) {                                   // if we just saved the downloaded file
                unlink(inetDnFilePath.c_str());                 // delete it from RPi
            }

            dataTrans->setStatus(FDD_RES_ONDEVICECOPY);
            return;
        } else {                                            // on device copy -- FAIL!
            Debug::out(LOG_DEBUG, "FloppySetup::searchDownloadOnDevice -- on device copy fail!");
        }
    } else {
        Debug::out(LOG_DEBUG, "FloppySetup::searchDownloadOnDevice -- can't do on device copy");
    }

    dataTrans->setStatus(FDD_ERROR);                        // if came here, on device copy didn't succeed
}

void FloppySetup::uploadStart(void)
{
    TranslatedDisk * translated = TranslatedDisk::getInstance();
    int index = cmd[5];

    if(index < 0 || index > 2) {                            // index out of range? fail
        dataTrans->setStatus(FDD_ERROR);
        return;
    }

    if(!translated) {                                       // can't work without this!
        return;
    }

    if(currentUpload.fh != NULL) {                          // if file was open but not closed, close it and don't copy it...
        fclose(currentUpload.fh);
    }

    memset(bfr64k, 0, 512);
    dataTrans->recvData(bfr64k, 512);                       // receive file name into this buffer
    std::string atariFilePath = (char *) bfr64k;
    std::string hostPath;
    std::string pathWithHostSeparators;

    // the following block should device, if on-device-copy of file is enough (when using translated drive), or should upload from ST
    bool doOnDeviceCopy = false;

    // try to convert atari path to host path (will work for translated drives, will fail for native)
    bool        waitingForMount, res;
    int         atariDriveIndex;
    std::string fullAtariPath;
    res = translated->createFullAtariPathAndFullHostPath(atariFilePath, fullAtariPath, atariDriveIndex, hostPath, waitingForMount);

    if(res) {                                               // good? file is on translated drive
        if(Utils::fileExists(hostPath)) {                   // file exists? do on-device-copy of file
            pathWithHostSeparators = hostPath;
            doOnDeviceCopy = true;
        }
    }

    if(!doOnDeviceCopy) {                                   // if we got here, then the file is either not on translated drive, or does not exist
        hostPath = "";

        pathWithHostSeparators = atariFilePath;             // convert atari path to host path separators
        translated->pathSeparatorAtariToHost(pathWithHostSeparators);
    }

    std::string path, file;
    Utils::splitFilenameFromPath(pathWithHostSeparators, path, file);

    path = FLOPPY_UPLOAD_PATH + file;

    FILE *f = NULL;

    if(!doOnDeviceCopy) {                                   // if not doing on-device-copy, try to open the file
        f = fopen((char *) path.c_str(), "wb");

        if(!f) {                                            // failed to open file?
            Debug::out(LOG_ERROR, "FloppySetup::uploadStart - failed to open file %s", (char *) path.c_str());

            dataTrans->setStatus(FDD_ERROR);
            return;
        }
    }

    // store params
    currentUpload.slotIndex             = index;
    currentUpload.fh                    = f;
    currentUpload.atariSourcePath       = atariFilePath;            // atari path:                      C:\bla.st
    currentUpload.hostPath              = hostPath;                 // host path for translated drives: /mnt/sda/bla.st, or /tmp/bla.st if it was uploaded from ST drive
    currentUpload.file                  = file;                     // just file name:                  bla.st

    // do on-device-copy if needed
    if(doOnDeviceCopy) {                                            // if doing on-device-copy...
        if(res) {                                                   // if file was copied
            cmd[4] = FDD_CMD_UPLOADIMGBLOCK_DONE_OK;
            cmd[5] = index;

            uploadEnd(true);                                        // pretend that we just uploaded it, this will also setStatus(...)
        } else {                                                    // if file copying failed, fail
            dataTrans->setStatus(FDD_ERROR);
        }
    } else {                                                        // if not doing on-device-copy, return with success
        dataTrans->setStatus(FDD_OK);
    }
}

void FloppySetup::uploadBlock(void)
{
    dataTrans->recvData(bfr64k, 64 * 1024);                 // get 64kB of data from ST

    uint8_t slotIndex  = cmd[5] >> 6;                          // get slot # to which the upload should go
    //uint8_t blockNo    = cmd[5] & 0x3f;

    if(slotIndex != currentUpload.slotIndex) {              // got open one slot, but trying to upload to another? fail!
        dataTrans->setStatus(FDD_ERROR);
        return;
    }

    // determine how much data should we write to file
    size_t blockSize = 0;
    uint8_t *ptr = NULL;

    if(cmd[4] == FDD_CMD_UPLOADIMGBLOCK_FULL) {             // get full block? transfer 64kB
        blockSize   = 64 * 1024;
        ptr         = bfr64k;
    } else if(cmd[4] == FDD_CMD_UPLOADIMGBLOCK_PART) {      // get partial block? Get block size as the 1st word in block
        blockSize   = (((uint16_t) bfr64k[0]) << 8) | ((uint16_t) bfr64k[1]);
        ptr         = bfr64k + 2;
    } else {                                                // this should never happen
        dataTrans->setStatus(FDD_ERROR);
        return;
    }

    // write the data
    size_t written = fwrite(ptr, 1, blockSize, currentUpload.fh);

    if(written != blockSize) {                              // when didn't write all the data, fail
        dataTrans->setStatus(FDD_ERROR);
        return;
    }

    dataTrans->setStatus(FDD_OK);                           // when all data was written
}

void FloppySetup::uploadEnd(bool isOnDeviceCopy)
{
    if(currentUpload.fh != 0) {                             // file was open?
        fclose(currentUpload.fh);                           // close it
        currentUpload.fh = NULL;
    }

    if(cmd[4] == FDD_CMD_UPLOADIMGBLOCK_DONE_FAIL) {        // if we're done with fail, don't do anything else
        dataTrans->setStatus(FDD_OK);
        return;
    }

    // we're here, the image upload succeeded, the following will also encode the image...
    shared.imageSilo->add(currentUpload.slotIndex, currentUpload.file, currentUpload.hostPath, currentUpload.atariSourcePath, true);

    // now finish with OK status
    if(isOnDeviceCopy) {                                        // for on-device-copy send special status
        dataTrans->setStatus(FDD_RES_ONDEVICECOPY);
    } else {                                                    // for normal upload send OK status
        dataTrans->setStatus(FDD_OK);
    }
}

void FloppySetup::newImage(void)
{
    int index = cmd[5];

    if(index < 0 || index > 2) {                        // index out of range? fail
        dataTrans->setStatus(FDD_ERROR);
        return;
    }

    if(currentUpload.fh != NULL) {                      // if file was open but not closed, close it and don't copy it...
        fclose(currentUpload.fh);
    }

    char file[128];
    getNewImageName(file);
    std::string justFile = file;
    std::string pathAndFile = FLOPPY_UPLOAD_PATH + justFile;

    bool res = createNewImage(pathAndFile);             // create the new image on disk

    if(!res) {                                          // failed to create? fail
        dataTrans->setStatus(FDD_ERROR);
        return;
    }

    // we're here, the image creation succeeded
    std::string empty;
    shared.imageSilo->add(index, justFile, pathAndFile, empty, true);

    dataTrans->setStatus(FDD_OK);
}

bool FloppySetup::createNewImage(std::string pathAndFile)
{
    // open the file
    FILE *f = fopen((char *) pathAndFile.c_str(), "wb");

    if(!f) {                                            // failed to open file?
        Debug::out(LOG_ERROR, "FloppySetup::newImage - failed to open file %s", (char *) pathAndFile.c_str());
        return false;
    }

    // create default boot sector (copied from blank .st image created in Steem)
    uint8_t sect0start[]   = {0xeb, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc8, 0x82, 0x75, 0x00, 0x02, 0x02, 0x01, 0x00, 0x02, 0x70, 0x00, 0xa0, 0x05, 0xf9, 0x05, 0x00, 0x09, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t sect0end[]     = {0x00, 0x97, 0xc7};

    uint8_t bfr[512];
    memset(bfr, 0, 512);

    memcpy(bfr, sect0start, sizeof(sect0start));                        // copy the start of default boot sector to start of buffer
    memcpy(bfr + 512 - sizeof(sect0end), sect0end, sizeof(sect0end));   // copy the end of default boot sector to end of buffer

    fwrite(bfr, 1, 512, f);

    // create the empty rest of the file
    memset(bfr, 0, 512);

    int totalSectorCount = (9*80*2) - 1;                                // calculate the count of sectors on a floppy - 2 sides, 80 tracks, 9 spt, minus the already written boot sector

    for(int i=0; i<totalSectorCount; i++) {
        fwrite(bfr, 1, 512, f);
    }

    fclose(f);
    return true;
}

void FloppySetup::getNewImageName(char *nameBfr)
{
    char fileName[24];

    for(int i=0; i<100; i++) {
        sprintf(fileName, "newimg%d.st", i);						// create new filename

        std::string fnameWithPath = FLOPPY_UPLOAD_PATH;
        fnameWithPath += fileName;									// this will be filename with path

        if(shared.imageSilo->containsImage(fileName)) {					// if this file is already in silo, skip it
            continue;
        }

        if(Utils::fileExists(fnameWithPath)) {                      // if this file does exist, delete it (it's not in silo)
            unlink(fnameWithPath.c_str());
        }

        break;														// break this cycle and thus use this filename
    }

    strcpy(nameBfr, fileName);
}

void FloppySetup::downloadStart(void)
{
    int index = cmd[5];
    std::string hostPath, justFileName;

    if(currentDownload.fh != NULL) {                        // previous download possibly not closed? close it now
        fclose(currentDownload.fh);
        currentDownload.fh = NULL;
    }

    if(index >=0 && index <=2) {                            // downloading from image slot?
        SiloSlot *ss = shared.imageSilo->getSiloSlot(index);       // get silo slot

        if(ss->imageFile.empty()) {                         // silo slot is empty?
            dataTrans->setStatus(FDD_ERROR);
            return;
        }

        hostPath        = ss->hostPath;                     // where on RPi is the file (e.g. /tmp/disk.img or /mnt/sda/gamez.bla.st)
        justFileName    = ss->imageFile;                    // just the filename (e.g. disk.img)
    } else if(index == 10) {                                // downloading from inet download file?
        hostPath        = inetDnFilePath;                   // where on RPi is the file (e.g. /tmp/A_001.st)
        justFileName    = inetDnFilename;                   // just the filename (e.g. A_001.st)
    } else {                                                // trying to download something other
        dataTrans->setStatus(FDD_ERROR);
        return;
    }

    FILE *fh = fopen(hostPath.c_str(), "rb");

    if(fh == NULL) {                                    // couldn't open file?
        dataTrans->setStatus(FDD_ERROR);
        return;
    }

    currentDownload.fh          = fh;
    currentDownload.imageFile   = justFileName;

    dataTrans->addDataBfr(justFileName.c_str(), justFileName.length() + 1, true);     // send filename to ST
    dataTrans->setStatus(FDD_OK);
}

#define DOWNLOAD_BLOCK_SIZE     (65536 - 2)

void FloppySetup::downloadGetBlock(void)
{
    int index   = cmd[5] >> 6;
    int blockNo = cmd[5] & 0x3f;

    if((index < 0 || index > 2) && index != 10) {       // index out of range? fail
        dataTrans->setStatus(FDD_ERROR);
        return;
    }

    if(currentDownload.fh == NULL) {                    // image not open? fao;
        dataTrans->setStatus(FDD_ERROR);
        return;
    }

    int offset = blockNo * DOWNLOAD_BLOCK_SIZE;         // calculate the offset

    fseek(currentDownload.fh, offset, SEEK_SET);        // seek to the right position

    size_t res = fread(bfr64k, 1, DOWNLOAD_BLOCK_SIZE, currentDownload.fh);     // read data into buffer

    if(res < 0) {                                       // in case of negative error code (or something), set to zero
        res = 0;
    }

    dataTrans->addDataWord((uint16_t) res);                         // first add the count of data that was read
    dataTrans->addDataBfr(bfr64k, DOWNLOAD_BLOCK_SIZE, true);   // then add the actual data
    dataTrans->setStatus(FDD_OK);                               // and return the good status
}

void FloppySetup::downloadDone(void)
{
    int index = cmd[5];

    if((index < 0 || index > 2) && index != 10) {       // index out of range? fail
        dataTrans->setStatus(FDD_ERROR);
        return;
    }

    if(currentDownload.fh != NULL) {                    // if file is open, close it
        fclose(currentDownload.fh);
        currentDownload.fh = NULL;
    }

    if(index == 10) {                                   // if we just saved the downloaded file
        unlink(inetDnFilePath.c_str());                 // delete it from RPi
    }

    dataTrans->setStatus(FDD_OK);
}

void FloppySetup::searchRefreshList(void)
{
    shared.imageList->refreshList();

    dataTrans->setStatus(FDD_OK);
}

void FloppySetup::getCurrentSlot(void)
{
    uint8_t currentSlot = shared.imageSilo->getCurrentSlot();     // get the current slot

    dataTrans->addDataByte(currentSlot);
    dataTrans->padDataToMul16();
    dataTrans->setStatus(FDD_OK);
}

void FloppySetup::setCurrentSlot(void)
{
    int newSlot = (int) cmd[5];

    shared.imageSilo->setCurrentSlot(newSlot);                 // set the slot for valid index, set the empty image for invalid slot
    dataTrans->setStatus(FDD_OK);

    if(reloadProxy) {                                   // if got settings reload proxy, invoke reload
        reloadProxy->reloadSettings(SETTINGSUSER_FLOPPY_SLOT);
    }
}

void FloppySetup::getImageEncodingRunning(void)
{
    static int progress = 0;
    char progChars[4] = {'|', '/', '-', '\\'};

    bool encoding = false;	// don't show encoding status - now encoding should not slow down floppy access. Was: ImageSilo::getFloppyEncodingRunning();
    bool doWeHaveStorage = shared.imageStorage->doWeHaveStorage();
    int  downloadCount = 0; // Downloader::count(DWNTYPE_FLOPPYIMG);
    int  downloadProgr = 0; // Downloader::progressOfCurrentDownload();

    dataTrans->addDataByte(encoding);            // is the encoding thread is encoding some image?
    dataTrans->addDataByte(doWeHaveStorage);     // do have image storage or not?
    dataTrans->addDataByte(downloadCount);       // count of items now downloading
    dataTrans->addDataByte(downloadProgr);       // download progress of current download

    std::string status;

    if(doWeHaveStorage) {               // if got storage
        if(encoding) {                  // if encoding
            status += std::string("Encoding");

            if(downloadCount > 0) {     // if also downloading, add column
                status += std::string(", ");
            } else {                    // not downloading, add heartbeat symbol
                status.push_back(' ');
                status.push_back(progChars[progress]);
                progress = (progress + 1) % 4;
            }
        }

        char tmp[50];
        if(downloadCount > 1) {         // more than 1 file?
            if(encoding) {              // and also encoding? shorter download status
                sprintf(tmp, "Download: %d files, now: %d %%", downloadCount, downloadProgr);
            } else {                    // not encoding? longer download status
                sprintf(tmp, "Downloading %d files, current: %d %%", downloadCount, downloadProgr);
            }

            status += tmp;
        } else if(downloadCount == 1) { // downloading 1 file?
            sprintf(tmp, "Downloading file: %d %%", downloadProgr);
            status += tmp;
        }
    } else {                            // don't have storage? add warning
        status += std::string("No USB or shared storage, ops limited!");
    }

    if(status.length() > 40) {          // if status is longer than screen line, shorten it
        status.resize(40);
    } else if(status.length() < 40) {   // if status is shorter than whole line, fill it overwrite whole previous line
        status.append(40 - status.length(), ' ');
    }

    dataTrans->addDataBfr(status.c_str(), status.length() + 1, true);   // add the status with terminating zero
    dataTrans->setStatus(FDD_OK);
}

void FloppySetup::logCmdName(uint8_t cmdCode)
{
    switch(cmdCode) {
        case FDD_CMD_IDENTIFY:                  Debug::out(LOG_DEBUG, "floppySetup command: FDD_CMD_IDENTIFY"); break;
        case FDD_CMD_GETSILOCONTENT:            Debug::out(LOG_DEBUG, "floppySetup command: FDD_CMD_GETSILOCONTENT"); break;

        case FDD_CMD_UPLOADIMGBLOCK_START:      Debug::out(LOG_DEBUG, "floppySetup command: FDD_CMD_UPLOADIMGBLOCK_START"); break;

        case FDD_CMD_UPLOADIMGBLOCK_FULL:       Debug::out(LOG_DEBUG, "floppySetup command: FDD_CMD_UPLOADIMGBLOCK_FULL"); break;
        case FDD_CMD_UPLOADIMGBLOCK_PART:       Debug::out(LOG_DEBUG, "floppySetup command: FDD_CMD_UPLOADIMGBLOCK_PART"); break;

        case FDD_CMD_UPLOADIMGBLOCK_DONE_OK:    Debug::out(LOG_DEBUG, "floppySetup command: FDD_CMD_UPLOADIMGBLOCK_DONE_OK"); break;
        case FDD_CMD_UPLOADIMGBLOCK_DONE_FAIL:  Debug::out(LOG_DEBUG, "floppySetup command: FDD_CMD_UPLOADIMGBLOCK_DONE_FAIL"); break;

        case FDD_CMD_SWAPSLOTS:                 Debug::out(LOG_DEBUG, "floppySetup command: FDD_CMD_SWAPSLOTS"); break;
        case FDD_CMD_REMOVESLOT:                Debug::out(LOG_DEBUG, "floppySetup command: FDD_CMD_REMOVESLOT"); break;

        case FDD_CMD_NEW_EMPTYIMAGE:            Debug::out(LOG_DEBUG, "floppySetup command: FDD_CMD_NEW_EMPTYIMAGE"); break;
        case FDD_CMD_GET_CURRENT_SLOT:          Debug::out(LOG_DEBUG, "floppySetup command: FDD_CMD_GET_CURRENT_SLOT"); break;
        case FDD_CMD_SET_CURRENT_SLOT:          Debug::out(LOG_DEBUG, "floppySetup command: FDD_CMD_SET_CURRENT_SLOT"); break;
        case FDD_CMD_GET_IMAGE_ENCODING_RUNNING: Debug::out(LOG_DEBUG, "floppySetup command: FDD_CMD_GET_IMAGE_ENCODING_RUNNING"); break;

        case FDD_CMD_DOWNLOADIMG_START:         Debug::out(LOG_DEBUG, "floppySetup command: FDD_CMD_DOWNLOADIMG_START"); break;
        case FDD_CMD_DOWNLOADIMG_ONDEVICE:      Debug::out(LOG_DEBUG, "floppySetup command: FDD_CMD_DOWNLOADIMG_ONDEVICE"); break;
        case FDD_CMD_DOWNLOADIMG_GETBLOCK:      Debug::out(LOG_DEBUG, "floppySetup command: FDD_CMD_DOWNLOADIMG_GETBLOCK"); break;
        case FDD_CMD_DOWNLOADIMG_DONE:          Debug::out(LOG_DEBUG, "floppySetup command: FDD_CMD_DOWNLOADIMG_DONE"); break;

        case FDD_CMD_SEARCH_INIT:               Debug::out(LOG_DEBUG, "floppySetup command: FDD_CMD_SEARCH_INIT"); break;
        case FDD_CMD_SEARCH_STRING:             Debug::out(LOG_DEBUG, "floppySetup command: FDD_CMD_SEARCH_STRING"); break;
        case FDD_CMD_SEARCH_RESULTS:            Debug::out(LOG_DEBUG, "floppySetup command: FDD_CMD_SEARCH_RESULTS"); break;
        case FDD_CMD_SEARCH_MARK:               Debug::out(LOG_DEBUG, "floppySetup command: FDD_CMD_SEARCH_MARK"); break;
        case FDD_CMD_SEARCH_DOWNLOAD:           Debug::out(LOG_DEBUG, "floppySetup command: FDD_CMD_SEARCH_DOWNLOAD"); break;
        case FDD_CMD_SEARCH_REFRESHLIST:        Debug::out(LOG_DEBUG, "floppySetup command: FDD_CMD_SEARCH_REFRESHLIST"); break;

        case FDD_CMD_SEARCH_DOWNLOAD2STORAGE:   Debug::out(LOG_DEBUG, "floppySetup command: FDD_CMD_SEARCH_DOWNLOAD2STORAGE"); break;
        case FDD_CMD_SEARCH_INSERT2SLOT:        Debug::out(LOG_DEBUG, "floppySetup command: FDD_CMD_SEARCH_INSERT2SLOT"); break;
        case FDD_CMD_SEARCH_DELETEFROMSTORAGE:  Debug::out(LOG_DEBUG, "floppySetup command: FDD_CMD_SEARCH_DELETEFROMSTORAGE"); break;

        default:                                Debug::out(LOG_DEBUG, "floppySetup command: ???"); break;
    }
}

