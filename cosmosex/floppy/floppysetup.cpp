#include <stdio.h>

#include "../global.h"
#include "../debug.h"
#include "../utils.h"
#include "../acsidatatrans.h"
#include "../translated/gemdos_errno.h"
#include "floppysetup.h"

#define UPLOAD_PATH "/tmp/"

FloppySetup::FloppySetup()
{
    dataTrans   = NULL;
    up          = NULL;
    imageSilo   = NULL;
    translated  = NULL;

    currentUpload.fh = NULL;
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
        Debug::out("FloppySetup::processCommand was called without valid dataTrans, can't tell ST that his went wrong...");
        return;
    }

    dataTrans->clear();                 // clean data transporter before handling

    if(imageSilo == 0) {
        Debug::out("FloppySetup::processCommand was called without valid imageSilo, can't do image setup stuff!");
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
        case FDD_CMD_UPLOADIMGBLOCK_DONE_FAIL:  uploadEnd();                break;

        case FDD_CMD_SWAPSLOTS:                 imageSilo->swap(cmd[5]);    break;
        case FDD_CMD_REMOVESLOT:                imageSilo->remove(cmd[5]);  break;

        case FDD_CMD_NEW_EMPTYIMAGE:                                        break;

        case FDD_CMD_DOWNLOADIMG_START:                                     break;
        case FDD_CMD_DOWNLOADIMG_GETBLOCK:                                  break;
        case FDD_CMD_DOWNLOADIMG_DONE:                                      break;
    }

    dataTrans->sendDataAndStatus();         // send all the stuff after handling, if we got any
}

void FloppySetup::uploadStart(void)
{
    int index = cmd[5];

    if(index < 0 || index > 2) {                        // index out of range? fail
        dataTrans->setStatus(FDD_ERROR);
        return;
    }

    if(!translated) {                                   // can't work without this!
        return;
    }

    if(currentUpload.fh != NULL) {                      // if file was open but not closed, close it and don't copy it...
        fclose(currentUpload.fh);
    }

    dataTrans->recvData(bfr64k, 512);                   // receive file name into this buffer
    std::string atariFilePath = (char *) bfr64k;
    std::string hostPath;
    std::string pathWithHostSeparators;

    // the following block should device, if on-device-copy of file is enough (when using translated drive), or should upload from ST
    bool doOnDeviceCopy = false;

    // try to convert atari path to host path (will work for translated drives, will fail for native)
    bool res = translated->createHostPath(atariFilePath, hostPath);

    if(res) {                                           // good? file is on translated drive
        if(translated->hostPathExists(hostPath)) {      // file exists? do on-device-copy of file
            pathWithHostSeparators = hostPath;
            doOnDeviceCopy = true;
        } 
    }
     
    if(!doOnDeviceCopy) {                               // if we got here, then the file is either not on translated drive, or does not exist
        hostPath = "";
        
        pathWithHostSeparators = atariFilePath;         // convert atari path to host path separators
        translated->pathSeparatorAtariToHost(pathWithHostSeparators);
    }

    std::string path, file;
    Utils::splitFilenameFromPath(pathWithHostSeparators, path, file);

    // TODO: do on-device copy if doOnDeviceCopy == true

    path = UPLOAD_PATH + file;

    FILE *f = fopen((char *) path.c_str(), "wb");

    if(!f) {                                            // failed to open file?
        Debug::out("FloppySetup::uploadStart - failed to open file %s", (char *) path.c_str());
    
        dataTrans->setStatus(FDD_ERROR);
        return;
    }

    // file was opened, store params, we got success
    currentUpload.slotIndex             = index;
    currentUpload.fh                    = f;
    currentUpload.atariSourcePath       = atariFilePath;            // atari path:                      C:\bla.st
    currentUpload.hostSourcePath        = hostPath;                 // host path for translated drives: /mnt/sda/bla.st
    currentUpload.hostDestinationPath   = path;                     // host destination:                /tmp/bla.st
    currentUpload.file                  = file;                     // just file name:                  bla.st

    dataTrans->setStatus(FDD_OK);
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

void FloppySetup::uploadEnd(void)
{
    if(currentUpload.fh != 0) {                             // file was open?
        fclose(currentUpload.fh);                           // close it
        currentUpload.fh = NULL;
    }

    if(cmd[4] == FDD_CMD_UPLOADIMGBLOCK_DONE_FAIL) {        // if we're done with fail, don't do anything else
        dataTrans->setStatus(FDD_OK);
        return;
    }

    // we're here, the image upload succeeded
    imageSilo->add(currentUpload.slotIndex, currentUpload.file, currentUpload.hostDestinationPath, currentUpload.atariSourcePath, currentUpload.hostSourcePath);

    // TODO: now convert the image to mfm stream so it could be used!

}


