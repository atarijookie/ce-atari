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
#include "update.h"
#include "utils.h"
#include "floppy/imagelist.h"

Versions Update::versions;

extern THwConfig hwConfig;

void Update::initialize(void)
{
    char appVersion[16];
    Version::getAppVersion(appVersion);

    Update::versions.app.fromString(                (char *) appVersion);
    Update::versions.xilinx.fromFirstLineOfFile(    (char *) XILINX_VERSION_FILE, false);   // xilinx version file without dashes
    Update::versions.imageList.fromFirstLineOfFile( (char *) IMAGELIST_LOCAL);
}

bool Update::createUpdateScript(bool withLinuxRestart, bool withForceXilinx)
{
    bool res;

    if(hwConfig.version < 3 && withForceXilinx) {   // for v1 and v2, and should force xilinx flash?
        res = writeSimpleTextFile(UPDATE_FLASH_XILINX, NULL);     // create this file to force xilinx flash

        if(!res) {              // if failed to create first file, just quit already
            return res;
        }
    }

    // write the main update script command
    // don't forget to pass 'nosystemctl dontkillcesuper' do ce_update.sh here, because we need those to tell ce_stop.sh (which is called in ce_update.sh)
    // that it shouldn't stop cesuper.sh, which we need to keep running to finish the update and restart ce_main_app back
    res = writeSimpleTextFile(UPDATE_SCRIPT, "#!/bin/sh\n/ce/ce_update.sh nosystemctl dontkillcesuper\n");

    if(!res) {                  // if failed to create first file, just quit already
        return res;
    }

    if(withLinuxRestart) {      // if should also restart linux, add reboot
        res = writeSimpleTextFile(UPDATE_REBOOT_FILE, NULL);
    }

    return res;
}

bool Update::createUpdateXilinxScript(void)
{
    bool res = createUpdateScript(false, true);     // don't reboot, force xilinx flash
    return res;
}

bool Update::createFlashFirstFwScript(bool withLinuxRestart)
{
    bool res = createUpdateScript(withLinuxRestart, false);     // reboot if requested, don't force xilinx flash
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

        found = Utils::fileExists(path);

        if(found) {                                             // found? good
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
    FILE *f = fopen(FDD_TEST_IMAGE_PATH_AND_FILENAME.c_str(), "wb");

    if(!f) {
        Debug::out(LOG_ERROR, "Failed to create floppy test image!");
        printf("Failed to create floppy test image!\n");
        return;
    }

    // first fill the write buffer with simple counter
    uint8_t writeBfr[512];
    int i;
    for(i=0; i<512; i++) {
        writeBfr[i] = (uint8_t) i;
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
