#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>

#include "global.h"
#include "debug.h"
#include "settings.h"
#include "downloader.h"
#include "update.h"
#include "utils.h"
#include "mounter.h"
#include "dir2fdd/cdirectory.h"
#include "floppy/imagelist.h"

Versions Update::versions;

extern THwConfig hwConfig;

volatile BYTE packageDownloadStatus     = DWNSTATUS_WAITING;
volatile BYTE updateListDownloadStatus  = DWNSTATUS_WAITING;

void Update::initialize(void)
{
    char appVersion[16];
    Version::getAppVersion(appVersion);

    Update::versions.current.app.fromString(                (char *) appVersion);
    Update::versions.current.xilinx.fromFirstLineOfFile(    (char *) XILINX_VERSION_FILE);
    Update::versions.current.imageList.fromFirstLineOfFile( (char *) IMAGELIST_LOCAL);
    Update::versions.updateListWasProcessed = false;
    Update::versions.gotUpdate              = false;
}

bool Update::createUpdateXilinxScript(void)
{
    FILE *f = fopen(UPDATE_SCRIPT, "wt");

    if(!f) {
        Debug::out(LOG_ERROR, "Update::createUpdateXilinxScript() failed to create update script - %s", UPDATE_SCRIPT);
        return false;
    }

    // execute the xilinx update script
    fprintf(f, "/ce/update/update_xilinx.sh \n");

    fclose(f);
    return true;
}

bool Update::createFlashFirstFwScript(bool withLinuxRestart)
{
    bool res = writeSimpleTextFile(UPDATE_SCRIPT, "#!/bin/sh\n/ce/ce_update.sh\n");

    if(withLinuxRestart) {                      // if should also restart linux, add reboot
        writeSimpleTextFile(UPDATE_REBOOT_FILE, NULL);
    }

    return res;
}

bool Update::checkForUpdateListOnUsb(std::string &updateFilePath)
{
    DIR *dir = opendir((char *) "/mnt/");                           // try to open the dir

    updateFilePath.clear();

    if(dir == NULL) {                                               // not found?
        Debug::out(LOG_DEBUG, "Update::checkForUpdateListOnUsb -- /mnt/ dir not found, fail");
        return false;
    }

    char path[512];
    memset(path, 0, 512);
    bool found = false;

    while(1) {                                                      // while there are more files, store them
        struct dirent *de = readdir(dir);                           // read the next directory entry

        if(de == NULL) {                                            // no more entries?
            break;
        }

        if(de->d_type != DT_DIR && de->d_type != DT_REG) {          // not a file, not a directory?
            continue;
        }

        if(strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
            continue;
        }

        // construct path
        strcpy(path, "/mnt/");
        strcat(path, de->d_name);
        strcat(path, "/");
        strcat(path, getUsbArchiveName());

        int res = access(path, F_OK);

        if(res != -1) {                                             // if it's not this error, then the file exists
            found = true;
            break;
        }
    }

    closedir(dir);

    if(found) {
        Debug::out(LOG_DEBUG, "Update::checkForUpdateListOnUsb -- update found: %s", path);
        updateFilePath = path;
        writeSimpleTextFile(UPDATE_USBFILE, path);              // also write this path to predefined file for ce_update.sh script
    } else {
        Debug::out(LOG_DEBUG, "Update::checkForUpdateListOnUsb -- update not found on usb");
    }

    return found;
}

const char *Update::getUsbArchiveName(void)
{
    #ifdef DISTRO_YOCTO
        return "yocto.zip";
    #endif

    #ifdef DISTRO_JESSIE
        return "jessie.zip";
    #endif

    #ifdef DISTRO_STRETCH
        return "stretch.zip";
    #endif

    return "unknown.zip";
}

const char *Update::getPropperXilinxTag(void)
{
    if(hwConfig.version == 1) {                 // v.1 ? it's xilinx
        return "xilinx";
    } else {                                    // v.2 ?
        if(hwConfig.hddIface == HDD_IF_ACSI) {  // v.2 and ACSI? it's xlnx2a
            return "xlnx2a";
        } else {                                // v.2 and SCSI? it's xlnx2s
            return "xlnx2s";
        }
    }

    return "??wtf??";
}

void Update::createFloppyTestImage(void)
{
    // open the file and write to it
    FILE *f = fopen(FDD_TEST_IMAGE_PATH_AND_FILENAME, "wb");

    if(!f) {
        Debug::out(LOG_ERROR, "Failed to create floppy test image!");
        printf("Failed to create floppy test image!\n");
        return;
    }

    // first fill the write buffer with simple counter
    BYTE writeBfr[512];
    int i;
    for(i=0; i<512; i++) {
        writeBfr[i] = (BYTE) i;
    }

    // write one sector after another...
    int sector, track, side;
    for(track=0; track<80; track++) {
        for(side=0; side<2; side++) {
            for(sector=1; sector<10; sector++) {
                // customize write data
                writeBfr[0] = track;
                writeBfr[1] = side;
                writeBfr[2] = sector;

                fwrite(writeBfr, 1, 512, f);
            }
        }
    }

    // close file and we're done
    fclose(f);
}

bool Update::writeSimpleTextFile(const char *path, const char *content)
{
    FILE *f = fopen(path, "wt");

    if(!f) {                // if couldn't open file, quit
        Debug::out(LOG_ERROR, "Update::writeSimpleTextFile failed to create file: %s", path);
        return false;
    }

    if(content) {           // if content specified, write it (otherwise empty file)
        fputs(content, f);
    }

    fclose(f);
    return true;
}

void Update::removeSimpleTextFile(const char *path)
{
    unlink(path);
}
