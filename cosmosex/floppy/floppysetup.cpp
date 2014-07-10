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

#define UPLOAD_PATH "/tmp/"

FloppySetup::FloppySetup()
{
    dataTrans   = NULL;
    up          = NULL;
    imageSilo   = NULL;
    translated  = NULL;

    currentUpload.fh = NULL;

    currentDownload.fh = NULL;

    imgDnStatus = IMG_DN_STATUS_IDLE;
}

FloppySetup::~FloppySetup()
{

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

    switch(cmd[4]) {
        case FDD_CMD_IDENTIFY:                          // return identification string
            dataTrans->addDataBfr((unsigned char *)"CosmosEx floppy setup", 21, true);       // add identity string with padding
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
    bool res = translated->createHostPath(atariFilePath, hostPath);

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
    std::string status;
    static int  retries = 0;
    
    if(imgDnStatus == IMG_DN_STATUS_DOWNLOADING) {          // if we're downloading
        Downloader::status(status, DWNTYPE_FLOPPYIMG);      // check if it's downloaded at this moment

        if(status.empty()) {                                // if no image is now downloaded, the image is downloaded, but it still might not be available yet
            int res = access(inetDnFilePath.c_str(), F_OK); // check if file exists

            if(res == 0) {                                  // exists
                imgDnStatus = IMG_DN_STATUS_DOWNLOADED;
                dataTrans->setStatus(FDD_DN_DONE);              // tell ST we got something to download
            } else {                                            // does not exist
                if(retries > 0) {
                    retries--;
            
                    status = inetDnFilename + ": verifying download";
            
                    dataTrans->addDataBfr((BYTE *) status.c_str(), status.length(), true);
                    dataTrans->setStatus(FDD_DN_WORKING);           // tell ST we're downloading
                } else {
                    imgDnStatus = IMG_DN_STATUS_IDLE;
                }
            }
        } else {                                            // some image is downloading
            std::replace(status.begin(), status.end(), '\n', ' '); // replace all new line characters with spaces

            dataTrans->addDataBfr((BYTE *) status.c_str(), status.length(), true);
            dataTrans->setStatus(FDD_DN_WORKING);           // tell ST we're downloading
        }
    }

    if(imgDnStatus == IMG_DN_STATUS_IDLE) {                 // if we're idle, start to download
        res = imageList.getFirstMarkedImage(url, checksum, filename);   // see if we got anything marked for download

        if(res) {                                           // if got some image to download, start the download
            retries = 5;    
    
            imgDnStatus     = IMG_DN_STATUS_DOWNLOADING;        // mark that we're downloading something
            inetDnFilename  = filename;                         // just filename of downloaded file    
            inetDnFilePath  = IMAGE_DOWNLOAD_DIR + filename;    // create full path and filename to the downloaded file

            unlink(inetDnFilePath.c_str());                     // if this file exists on the drive, delete it
            //------

            TDownloadRequest tdr;
            tdr.srcUrl          = url;
            tdr.checksum        = checksum;
            tdr.dstDir          = IMAGE_DOWNLOAD_DIR;
            tdr.downloadType    = DWNTYPE_FLOPPYIMG;        // we're downloading floppy image
            Downloader::add(tdr);
            
            status = "Downloading: " + url;
            dataTrans->addDataBfr((BYTE *) status.c_str(), status.length(), true);

            dataTrans->setStatus(FDD_DN_WORKING);           // tell ST we're downloading
        } else {                                            // nothing to download? say that to ST
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
    bool res = translated->createHostPath(atariFilePath, hostPath);

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
    std::string fileStr = file;

    std::string path = UPLOAD_PATH + fileStr;

    // open the file
    FILE *f = fopen((char *) path.c_str(), "wb");

    if(!f) {                                            // failed to open file?
        Debug::out(LOG_ERROR, "FloppySetup::newImage - failed to open file %s", (char *) path.c_str());
    
        dataTrans->setStatus(FDD_ERROR);
        return;
    }

    // create default boot sector (copied from blank .st image created in Steem)
    char sect0start[]   = {0xeb, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc8, 0x82, 0x75, 0x00, 0x02, 0x02, 0x01, 0x00, 0x02, 0x70, 0x00, 0xa0, 0x05, 0xf9, 0x05, 0x00, 0x09, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00};
    char sect0end[]     = {0x00, 0x97, 0xc7};

    char bfr[512];
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

    // we're here, the image creation succeeded
    std::string empty;
    imageSilo->add(index, fileStr, path, empty, empty, true);

    dataTrans->setStatus(FDD_OK);
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

    dataTrans->addDataBfr((BYTE *) justFileName.c_str(), justFileName.length() + 1, true);     // send filename to ST
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


