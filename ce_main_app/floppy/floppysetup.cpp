#include <stdio.h>
#include <string.h>

#include <unistd.h>

#include <algorithm>

#include "../global.h"
#include "../debug.h"
#include "../utils.h"
#include "../acsidatatrans.h"
#include "../translated/gemdos_errno.h"
#include "../downloader.h"
#include "floppysetup.h"
#include "floppysetup_commands.h"

#define UPLOAD_PATH "/tmp/"

volatile BYTE currentImageDownloadStatus;

FloppySetup::FloppySetup()
{
    dataTrans   = NULL;
    up          = NULL;
    imageSilo   = NULL;
    translated  = NULL;
    reloadProxy = NULL;

    currentUpload.fh = NULL;

    currentDownload.fh = NULL;

    imgDnStatus = IMG_DN_STATUS_IDLE;
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

void FloppySetup::setImageSilo(ImageSilo *imgSilo)
{
    imageSilo = imgSilo;
}

void FloppySetup::setTranslatedDisk(TranslatedDisk *td)
{
    translated = td;
}

void FloppySetup::processCommand(BYTE *command)
{
    cmd = command;

    if(dataTrans == 0) {
        Debug::out(LOG_ERROR, "FloppySetup::processCommand was called without valid dataTrans, can't tell ST that his went wrong...");
        return;
    }

    dataTrans->clear();                 // clean data transporter before handling

    if(imageSilo == 0) {
        Debug::out(LOG_ERROR, "FloppySetup::processCommand was called without valid imageSilo, can't do image setup stuff!");
        dataTrans->setStatus(FDD_ERROR);
        dataTrans->sendDataAndStatus();
        return;
    }

    logCmdName(cmd[4]);
    
    switch(cmd[4]) {
        case FDD_CMD_IDENTIFY:                          // return identification string
            dataTrans->addDataBfr("CosmosEx floppy setup", 21, true);       // add identity string with padding
            dataTrans->setStatus(FDD_OK);
            break;

        case FDD_CMD_GETSILOCONTENT:                    // return the filenames and contents of current floppy images in silo
            BYTE bfr[512];
            imageSilo->dumpStringsToBuffer(bfr);

            dataTrans->addDataBfr(bfr, 512, true);
            dataTrans->setStatus(FDD_OK);
            break;

        case FDD_CMD_UPLOADIMGBLOCK_START:      uploadStart();              break;

        case FDD_CMD_UPLOADIMGBLOCK_FULL:       
        case FDD_CMD_UPLOADIMGBLOCK_PART:       uploadBlock();              break;

        case FDD_CMD_UPLOADIMGBLOCK_DONE_OK:    
        case FDD_CMD_UPLOADIMGBLOCK_DONE_FAIL:  uploadEnd(false);           break;

        case FDD_CMD_SWAPSLOTS:                 imageSilo->swap(cmd[5]);    dataTrans->setStatus(FDD_OK);	break;
        case FDD_CMD_REMOVESLOT:                imageSilo->remove(cmd[5]);  dataTrans->setStatus(FDD_OK);	break;

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
        case FDD_CMD_SEARCH_MARK:               searchMark();               break;
        case FDD_CMD_SEARCH_DOWNLOAD:           searchDownload();           break;
        case FDD_CMD_SEARCH_REFRESHLIST:        searchRefreshList();        break;
    }

    dataTrans->sendDataAndStatus();         // send all the stuff after handling, if we got any
}

void FloppySetup::searchInit(void)
{
    if(!imageList.exists()) {               // if the file does not yet exist, tell ST that we're downloading
        dataTrans->setStatus(FDD_DN_LIST);
        return;
    }

    if(!imageList.loadList()) {             // try to load the list, if failed, error
        dataTrans->setStatus(FDD_ERROR);
        return;
    }

    dataTrans->setStatus(FDD_OK);           // done
}

void FloppySetup::searchString(void)
{
    dataTrans->recvData(bfr64k, 512);       // get one sector from ST

    imageList.search((char *) bfr64k);      // try to search for this string

    dataTrans->setStatus(FDD_OK);           // done
}

#define PAGESIZE    15
void FloppySetup::searchResult(void)
{
    int page = cmd[5];                              // retrieve # of page (0 .. max page - 1)

    int pageStart   = page * PAGESIZE;              // starting index of this page
    int pageEnd     = (page + 1) * PAGESIZE;        // ending index of this page (actually start of new page)

    int results = imageList.getSearchResultsCount();

    pageStart   = MIN(pageStart,    results);
    pageEnd     = MIN(pageEnd,      results);
    
    int realPage    = pageStart / PAGESIZE;         // calculate the real page number
    int totalPages  = (results   / PAGESIZE) + 1;   // calculate the count of pages we have
    
    dataTrans->addDataByte((BYTE) realPage);        // byte 0: real page
    dataTrans->addDataByte((BYTE) totalPages);      // byte 1: total pages

    memset(bfr64k, 0, 1024);

    // now get the search results - 68 bytes per line
    int offset = 0;
    for(int i=pageStart; i<pageEnd; i++) {
        imageList.getResultByIndex(i, (char *) (bfr64k + offset));
        offset += 68;
    }

    dataTrans->addDataBfr(bfr64k, 15 * 68, true);
    dataTrans->setStatus(FDD_OK);                   // done
}

void FloppySetup::searchMark(void)
{
    dataTrans->recvData(bfr64k, 512);               // read data

    int page = (int) bfr64k[0];
    int item = (int) bfr64k[1];

    int itemIndex = (page * PAGESIZE) + item;

    imageList.markImage(itemIndex);                 // (un)mark this image

    dataTrans->setStatus(FDD_OK);                   // done
}

void FloppySetup::downloadOnDevice(void)
{
    int index = cmd[5];

	memset(bfr64k, 0, 512);
    dataTrans->recvData(bfr64k, 512);                       // receive file name into this buffer
    std::string atariFilePath = (char *) bfr64k;
    std::string hostPath;
    std::string pathWithHostSeparators;

    // try to convert atari path to host path (will work for translated drives, will fail for native)
    bool        waitingForMount, res;
    int         atariDriveIndex, zipDirNestingLevel;
    std::string fullAtariPath;
    res = translated->createFullAtariPathAndFullHostPath(atariFilePath, fullAtariPath, atariDriveIndex, hostPath, waitingForMount, zipDirNestingLevel);
    
    if(res) {                                               // good? file is on translated drive
        res = Utils::copyFile(currentDownload.fh, hostPath);

        if(res) {                                           // on device copy -- GOOD!
            Debug::out(LOG_DEBUG, "FloppySetup::searchDownloadOnDevice -- on device copy success!");

            fclose(currentDownload.fh);                     // close the previously opened file
            currentDownload.fh = NULL;

            if(index == 10) {                                   // if we just saved the downloaded file
                unlink(inetDnFilePath.c_str());                 // delete it from RPi
                imgDnStatus = IMG_DN_STATUS_IDLE;               // the download is now idle
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

void FloppySetup::searchDownload(void)
{
    std::string url, filename;
    int         checksum;
    bool        res;
    std::string statusStr;
    BYTE        statusVal;
    
    statusStr.clear();                                      // clear status - might contain something in case of DOWNLOAD_FAIL
    
    if(imgDnStatus == IMG_DN_STATUS_DOWNLOADING) {          // if we're downloading
        switch(currentImageDownloadStatus) {
            case DWNSTATUS_WAITING:                         // in this state just report we're working
                Debug::out(LOG_DEBUG, "FloppySetup::searchDownload -- DWNSTATUS_WAITING");
                
                statusStr = inetDnFilename + ": waiting for start of download";
                statusVal = FDD_DN_WORKING;
                break;
                
            case DWNSTATUS_DOWNLOADING:                     // in this state just report we're working
                Debug::out(LOG_DEBUG, "FloppySetup::searchDownload -- DWNSTATUS_DOWNLOADING");

                Downloader::status(statusStr, DWNTYPE_FLOPPYIMG);    
                std::replace(statusStr.begin(), statusStr.end(), '\n', ' '); // replace all new line characters with spaces                
                statusVal = FDD_DN_WORKING;
                break;
                
            case DWNSTATUS_VERIFYING:                       // in this state just report we're working
                Debug::out(LOG_DEBUG, "FloppySetup::searchDownload -- DWNSTATUS_VERIFYING");

                statusStr = inetDnFilename + ": verifying checksum";
                statusVal = FDD_DN_WORKING;
                break;
                
            case DWNSTATUS_DOWNLOAD_OK:                     // report we've downloaded stuff, and go to DOWNLOADED state
                Debug::out(LOG_DEBUG, "FloppySetup::searchDownload -- DWNSTATUS_DOWNLOAD_OK");

                statusStr = inetDnFilename + ": download OK";
                statusVal = FDD_DN_DONE;
                
                imgDnStatus = IMG_DN_STATUS_DOWNLOADED;     // go to this state
                break;
                
            case DWNSTATUS_DOWNLOAD_FAIL:                   // when failed, report that we've failed and go to next download
                Debug::out(LOG_DEBUG, "FloppySetup::searchDownload -- DWNSTATUS_DOWNLOAD_FAIL");

                statusStr = inetDnFilename + ": download failed!\n\r\n\r";
                
                imgDnStatus = IMG_DN_STATUS_IDLE;           // go to this state                           
                break;

            default:                                        // this should never happen
                Debug::out(LOG_DEBUG, "FloppySetup::searchDownload -- default, wtf?");

                statusStr = "WTF?";
                break;
        }
        
        if(imgDnStatus != IMG_DN_STATUS_IDLE) {             // if it's not the case of failed download
            Debug::out(LOG_DEBUG, "FloppySetup::searchDownload -- not IMG_DN_STATUS_IDLE, status: %s", statusStr.c_str());

            dataTrans->addDataBfr(statusStr.c_str(), statusStr.length(), true);
            dataTrans->setStatus(statusVal);
            return;
        }
    }

    // if we came here, we either haven't been downloading yet, or the previous download failed
    if(imgDnStatus == IMG_DN_STATUS_IDLE) {                 // if we're idle, start to download
        Debug::out(LOG_DEBUG, "FloppySetup::searchDownload -- is IMG_DN_STATUS_IDLE");

        res = imageList.getFirstMarkedImage(url, checksum, filename);   // see if we got anything marked for download

        if(res) {                                           // if got some image to download, start the download
            Debug::out(LOG_DEBUG, "FloppySetup::searchDownload -- getFirstMarkedImage() returned %s, will download it", filename.c_str());
            
            imgDnStatus     = IMG_DN_STATUS_DOWNLOADING;        // mark that we're downloading something
            inetDnFilename  = filename;                         // just filename of downloaded file    
            inetDnFilePath  = IMAGE_DOWNLOAD_DIR + filename;    // create full path and filename to the downloaded file

            unlink(inetDnFilePath.c_str());                     // if this file exists on the drive, delete it
            //------
            currentImageDownloadStatus = DWNSTATUS_WAITING;
            
            TDownloadRequest tdr;
            tdr.srcUrl          = url;
            tdr.checksum        = checksum;
            tdr.dstDir          = IMAGE_DOWNLOAD_DIR;
            tdr.downloadType    = DWNTYPE_FLOPPYIMG;            // we're downloading floppy image
            tdr.pStatusByte     = &currentImageDownloadStatus;  // update this variable with current status
            Downloader::add(tdr);
            
            statusStr += "Downloading: " + url;
            dataTrans->addDataBfr(statusStr.c_str(), statusStr.length(), true);

            dataTrans->setStatus(FDD_DN_WORKING);           // tell ST we're downloading
        } else {                                            // nothing to download? say that to ST
            Debug::out(LOG_DEBUG, "FloppySetup::searchDownload -- nothing (more) to download");

            dataTrans->setStatus(FDD_DN_NOTHING_MORE);
        }
    }
}

void FloppySetup::uploadStart(void)
{
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
    int         atariDriveIndex, zipDirNestingLevel;
    std::string fullAtariPath;
    res = translated->createFullAtariPathAndFullHostPath(atariFilePath, fullAtariPath, atariDriveIndex, hostPath, waitingForMount, zipDirNestingLevel);

    if(res) {                                               // good? file is on translated drive
        if(translated->hostPathExists(hostPath)) {          // file exists? do on-device-copy of file
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

    path = UPLOAD_PATH + file;
	
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
    currentUpload.hostSourcePath        = hostPath;                 // host path for translated drives: /mnt/sda/bla.st
    currentUpload.hostDestinationPath   = path;                     // host destination:                /tmp/bla.st
    currentUpload.file                  = file;                     // just file name:                  bla.st

    // do on-device-copy if needed
    if(doOnDeviceCopy) {                                            // if doing on-device-copy...
        res = Utils::copyFile(hostPath, path);                      // copy the file

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

    BYTE slotIndex  = cmd[5] >> 6;                          // get slot # to which the upload should go
    //BYTE blockNo    = cmd[5] & 0x3f;                    

    if(slotIndex != currentUpload.slotIndex) {              // got open one slot, but trying to upload to another? fail!
        dataTrans->setStatus(FDD_ERROR);
        return;
    }

    // determine how much data should we write to file
    size_t blockSize = 0;
    BYTE *ptr = NULL;

    if(cmd[4] == FDD_CMD_UPLOADIMGBLOCK_FULL) {             // get full block? transfer 64kB
        blockSize   = 64 * 1024;
        ptr         = bfr64k;
    } else if(cmd[4] == FDD_CMD_UPLOADIMGBLOCK_PART) {      // get partial block? Get block size as the 1st word in block
        blockSize   = (((WORD) bfr64k[0]) << 8) | ((WORD) bfr64k[1]);
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
    imageSilo->add(currentUpload.slotIndex, currentUpload.file, currentUpload.hostDestinationPath, currentUpload.atariSourcePath, currentUpload.hostSourcePath, true);

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
    std::string pathAndFile = UPLOAD_PATH + justFile;

    bool res = createNewImage(pathAndFile);             // create the new image on disk

    if(!res) {                                          // failed to create? fail
        dataTrans->setStatus(FDD_ERROR);
        return;
    }

    // we're here, the image creation succeeded
    std::string empty;
    imageSilo->add(index, justFile, pathAndFile, empty, empty, true);

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
    BYTE sect0start[]   = {0xeb, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc8, 0x82, 0x75, 0x00, 0x02, 0x02, 0x01, 0x00, 0x02, 0x70, 0x00, 0xa0, 0x05, 0xf9, 0x05, 0x00, 0x09, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00};
    BYTE sect0end[]     = {0x00, 0x97, 0xc7};

    BYTE bfr[512];
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
		
		std::string fnameWithPath = UPLOAD_PATH;
		fnameWithPath += fileName;									// this will be filename with path
		
		if(imageSilo->containsImage(fileName)) {					// if this file is already in silo, skip it
			continue;
		}
		
		if(translated->hostPathExists(fnameWithPath)) {				// if this file does exist, delete it (it's not in silo)
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
        SiloSlot *ss = imageSilo->getSiloSlot(index);       // get silo slot

        if(ss->imageFile.empty()) {                         // silo slot is empty?
            dataTrans->setStatus(FDD_ERROR);
            return;
        }

        hostPath        = ss->hostDestPath;                 // where on RPi is the file (e.g. /tmp/disk.img)
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

    dataTrans->addDataWord((WORD) res);                         // first add the count of data that was read
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
        imgDnStatus = IMG_DN_STATUS_IDLE;               // the download is now idle
    }

    dataTrans->setStatus(FDD_OK);
}

void FloppySetup::searchRefreshList(void)
{
    imageList.refreshList();

    dataTrans->setStatus(FDD_OK);
}

void FloppySetup::getCurrentSlot(void)
{
    BYTE currentSlot = imageSilo->getCurrentSlot();     // get the current slot

    dataTrans->addDataByte(currentSlot);
    dataTrans->padDataToMul16();
    dataTrans->setStatus(FDD_OK);
}

void FloppySetup::setCurrentSlot(void)
{
    int newSlot = (int) cmd[5];

    imageSilo->setCurrentSlot(newSlot);                 // set the slot for valid index, set the empty image for invalid slot
    dataTrans->setStatus(FDD_OK);
    
    if(reloadProxy) {                                   // if got settings reload proxy, invoke reload
        reloadProxy->reloadSettings(SETTINGSUSER_FLOPPY_SLOT);
    }
}

void FloppySetup::getImageEncodingRunning(void)
{
    BYTE encondingRunning;

    encondingRunning = ImageSilo::getFloppyEncodingRunning() ? 1 : 0;

    dataTrans->addDataByte(encondingRunning);           // return if the encoding thread is encoding some image
    dataTrans->padDataToMul16();
    dataTrans->setStatus(FDD_OK);
}

void FloppySetup::logCmdName(BYTE cmdCode)
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
        
        default:                                Debug::out(LOG_DEBUG, "floppySetup command: ???"); break;
    }
}

