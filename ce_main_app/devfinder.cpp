// vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab
#include <stdio.h>
#include <string.h>

#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define PATH_BUFF_SIZE      1024

#include "translated/translatedhelper.h"
#include "translated/translateddisk.h"
#include "devfinder.h"
#include "utils.h"
#include "debug.h"
#include "mounter.h"
#include "native/scsi.h"

#include "periodicthread.h"

extern SharedObjects shared;

DevFinder::DevFinder()
{

}

void DevFinder::lookForDevChanges(void)
{
    char linkBuf[PATH_BUFF_SIZE];
    char devBuf[PATH_BUFF_SIZE];
    const char * devpath;
    
    someDevChanged = false;
    devpath = shared.mountRawNotTrans ? DISK_LINKS_PATH_ID : DISK_LINKS_PATH_UUID;
    
    DIR *dir = opendir(devpath);                            // try to open the dir
    
    if(dir == NULL) {                                               // not found?
        return;
    }

    clearFoundFlags();                                              // mark all items in map as not found yet
    
    while(1) {                                                      // while there are more files, store them
        struct dirent *de = readdir(dir);                           // read the next directory entry
    
        if(de == NULL) {                                            // no more entries?
            break;
        }

        if(de->d_type != DT_LNK) {                                  // if it's not a link, skip it
            continue;
        }

        memset(linkBuf, 0, PATH_BUFF_SIZE);
        memset(devBuf,  0, PATH_BUFF_SIZE);
        
        strcpy(linkBuf, devpath);
        strcat(linkBuf, HOSTPATH_SEPAR_STRING);
        strcat(linkBuf, de->d_name);
        
        int ires = readlink(linkBuf, devBuf, PATH_BUFF_SIZE);       // try to resolve the filename from the link
        if(ires == -1) {
            continue;
        }
        
        std::string pathAndFile = devBuf;
        std::string path, file;
        
        Utils::splitFilenameFromPath(pathAndFile, path, file);      // get only file name, skip the path (which now is something like '../../')
        
        if(file.find("mmcblk") != std::string::npos) {              // and if it's SD card (the one from which RPi boots), skip it
            continue;
        }

#ifdef ONPC
        if(file.substr(0,2)=="sd" ) {               // don't mount SCSI device on PC
            continue;
        }
        
                //enable loop device mounting (e.g. loop0) on PC)
        if(file.find("loop") == std::string::npos) {    
                    cutBeforeFirstNumber(file);                                 // cut before the 1st number (e.g. sda1 -> sda)
        }
#else
                cutBeforeFirstNumber(file);                                 // cut before the 1st number (e.g. sda1 -> sda)
#endif                
        file = "/dev/" + file;                                      // create full path - /dev/sda
        
        processFoundDev(file);                                      // and do something with that file
    }

    closedir(dir);  
    
    findAndSignalDettached();                                       // now go through the mapDevToFound and see what disappeared
    
//  if(!someDevChanged) {
//      Debug::out("no change");
//  }
}

void DevFinder::getDevPartitions(std::string devName, std::list<std::string> &partitions)
{
    partitions.erase(partitions.begin(), partitions.end());     // erease list content
    
    char linkBuf[PATH_BUFF_SIZE];
    char devBuf[PATH_BUFF_SIZE];

    DIR *dir = opendir(DISK_LINKS_PATH_ID);                         // try to open the dir
    
    if(dir == NULL) {                                               // not found?
        return;
    }

    while(1) {                                                      // while there are more files, store them
        struct dirent *de = readdir(dir);                           // read the next directory entry
    
        if(de == NULL) {                                            // no more entries?
            break;
        }

        if(de->d_type != DT_LNK) {                                  // if it's not a link, skip it
            continue;
        }

        memset(linkBuf, 0, PATH_BUFF_SIZE);
        memset(devBuf,  0, PATH_BUFF_SIZE);
        
        strcpy(linkBuf, DISK_LINKS_PATH_ID);
        strcat(linkBuf, HOSTPATH_SEPAR_STRING);
        strcat(linkBuf, de->d_name);
        
        int ires = readlink(linkBuf, devBuf, PATH_BUFF_SIZE);       // try to resolve the filename from the link
        if(ires == -1) {
            continue;
        }
        
        std::string pathAndFile = devBuf;
        std::string path, file;
        
        Utils::splitFilenameFromPath(pathAndFile, path, file);      // get only file name, skip the path
        file = "/dev/" + file;                                      // create full path - /dev/sda
        
        if(file.length() <= devName.length()) {                     // if what we've found is shorter or equaly long as what we are looking for, skip it
            continue;
        }
        
        if(file.find(devName) == 0) {                               // if the file name starts with the device
            partitions.push_back(file);
        }
    }

    closedir(dir);  
    
    partitions.sort();                                              // sort them alphabetically
    
    if(partitions.size() == 0) {                                    // if we didn't find partitions on that device (e.g. sda1, sda2), then add the whole disk (sda) - it might be without partitions, just one partition
        Debug::out(LOG_DEBUG, "DevFinder::getDevPartitions -- no partitions found on the device %s, using whole device...", devName.c_str());
        partitions.push_back(devName);
    }
}

void DevFinder::processFoundDev(std::string file)
{
    std::map<std::string, bool>::iterator it;
    it = mapDevToFound.find(file);                                  // try to find the device in the map
    
    if(it == mapDevToFound.end()) {                                 // don't have this device? add it, signal it
        mapDevToFound.insert( std::pair<std::string, bool>(file, true) );
        someDevChanged = true;
        
        bool atariDrive = isAtariDrive(file);
        
        Debug::out(LOG_DEBUG, "device attached: %s, is atari drive: %d", file.c_str(), atariDrive);     // write out

        onDevAttached(file, atariDrive);
    } else {                                                        // have this device? just mark it as found again
        it->second = true;
    }   
}

void DevFinder::cutBeforeFirstNumber(std::string &filename)
{
    size_t i;
    
    for(i=0; i<filename.length(); i++) {                        // go through the file name looking for a number
        if(filename[i] >= '0' && filename[i] <= '9') {          // if this is a number, cut it here
            filename.resize(i);
            return;
        }   
    }
}

void DevFinder::clearMap(void)                                  // called to make all the devices appear as new
{
    mapDevToFound.clear();
}

void DevFinder::clearFoundFlags(void)                           // called to clear just the found flags, so we can tell new and old devices apart
{
    std::map<std::string, bool>::iterator it;                           // set all found flags to false

    for(it = mapDevToFound.begin(); it != mapDevToFound.end(); ++it) {
        it->second = false;
    }
}

void DevFinder::findAndSignalDettached(void)
{
    std::map<std::string, bool>::iterator it, del;                          // set all found flags to false

    del = mapDevToFound.end();
    
    for(it = mapDevToFound.begin(); it != mapDevToFound.end(); ++it) {      // go through the map and look for false found flag
        if(del != mapDevToFound.end()) {                                    // if we should erase something, erase it
            mapDevToFound.erase(del);
            del = mapDevToFound.end();
        }
        
        if(it->second == false) {                                           // not found? it was detached
            del = it;                                                       // delete dev in next step
            someDevChanged = true;
            
            Debug::out(LOG_DEBUG, "device detached: %s", it->first.c_str());

            onDevDetached(it->first);
        }
    }
    
    if(del != mapDevToFound.end()) {                                        // if we should erase something, erase it
        mapDevToFound.erase(del);
    }
}

bool DevFinder::isAtariDrive(std::string file)
{
    if(geteuid() != 0) {
        Debug::out(LOG_ERROR, "Warning! Not running as root, won't be able to check dev partitions types!");
    }
    
    std::string fullPath = "/dev/" + file;
    char bfr[512];
    
    int fdes = open(fullPath.c_str(), O_RDONLY);                    // try to open the device

    if (fdes < 0) {                                                         // failed to open?
        return false;
    }
    
    int cnt = read(fdes, bfr, 512);                                         // try to read sector #0
    close(fdes);                                                            // close device
    
    if(cnt < 512) {                                                         // failed?
        return false;
    }
    
    if(isAtariPartitionType(&bfr[0x1c6])) {                                 // check partition header
        return true;
    }
    
    if(isAtariPartitionType(&bfr[0x1d2])) {                                 // check partition header
        return true;
    }
    
    if(isAtariPartitionType(&bfr[0x1de])) {                                 // check partition header
        return true;
    }
    
    if(isAtariPartitionType(&bfr[0x1ea])) {                                 // check partition header
        return true;
    }

    return false;                                                           // no atari partition found 
}

bool DevFinder::isAtariPartitionType(char *bfr) 
{
    if((bfr[0] & 0x01) == 0) {              // partition exists flag not set?
        return false;
    }

    if(strncmp(bfr + 1, "GEM", 3) == 0) {   // it's a GEM partition?
        return true;
    }

    if(strncmp(bfr + 1, "BGM", 3) == 0) {   // it's a BGM partition?
        return true;
    }

    if(strncmp(bfr + 1, "XGM", 3) == 0) {   // it's a XGM partition?
        return true;
    }
    
    return false;                           // no atari partition found
}

void DevFinder::onDevAttached(std::string devName, bool isAtariDrive)
{
    Debug::out(LOG_DEBUG, "DevFinder::onDevAttached: devName %s", devName.c_str());

    pthread_mutex_lock(&shared.mtxScsi);
   
    if(shared.mountRawNotTrans) {           // attach as raw?
        Debug::out(LOG_DEBUG, "DevFinder::onDevAttached -- should mount USB media as raw, attaching as RAW");
        shared.scsi->attachToHostPath(devName, SOURCETYPE_DEVICE, SCSI_ACCESSTYPE_FULL);
    } else {                                // attach as translated?
        Debug::out(LOG_DEBUG, "DevFinder::onDevAttached -- should mount USB media as translated, attaching as TRANSLATED");
        attachDevAsTranslated(devName);
    }
    
    pthread_mutex_unlock(&shared.mtxScsi);
}

void DevFinder::onDevDetached(std::string devName)
{
    pthread_mutex_lock(&shared.mtxScsi);

    // try to detach the device - works if was attached as RAW, does nothing otherwise
    shared.scsi->dettachFromHostPath(devName);

    // and also try to detach the device from translated disk
    std::pair <std::multimap<std::string, std::string>::iterator, std::multimap<std::string, std::string>::iterator> ret;
    std::multimap<std::string, std::string>::iterator it;

    ret = mapDeviceToHostPaths.equal_range(devName);                // find a range of host paths which are mapped to partitions found on this device

    TranslatedDisk * translated = TranslatedDisk::getInstance();
    if(translated) {
        translated->mutexLock();
        for (it = ret.first; it != ret.second; ++it) {                  // now go through the list of device - host_path pairs and unmount them
            std::string hostPath = it->second;                          // retrieve just the host path
            translated->detachFromHostPath(hostPath);           // now try to detach this from translated drives
        }
        translated->mutexUnlock();
    }

    mapDeviceToHostPaths.erase(ret.first, ret.second);              // and delete the whole device items from this multimap
    
    pthread_mutex_unlock(&shared.mtxScsi);
}

void DevFinder::attachDevAsTranslated(std::string devName)
{
    bool res;
    std::list<std::string>              partitions;
    std::list<std::string>::iterator    it;

    getDevPartitions(devName, partitions);                                      // get list of partitions for that device (sda -> sda1, sda2)

    for (it = partitions.begin(); it != partitions.end(); ++it) {               // go through those partitions
        std::string partitionDevice;
        std::string mountPath;
        std::string devPath, justDevName;

        partitionDevice = *it;                                                  // get the current device, which represents single partition (e.g. sda1)

        Utils::splitFilenameFromPath(partitionDevice, devPath, justDevName);    // split path to path and device name (e.g. /dev/sda1 -> /dev + sda1)
        mountPath = "/mnt/" + justDevName;                                      // create host path (e.g. /mnt/sda1)

        TMounterRequest tmr;
        tmr.action          = MOUNTER_ACTION_MOUNT;                             // action: mount
        tmr.deviceNotShared = true;                                             // mount as device
        tmr.devicePath      = partitionDevice;                                  // e.g. /dev/sda2
        tmr.mountDir        = mountPath;                                        // e.g. /mnt/sda2
        Mounter::add(tmr);

        res = TranslatedDisk::getInstance()->attachToHostPath(mountPath, TRANSLATEDTYPE_NORMAL, partitionDevice);   // try to attach

        if(!res) {                                                              // if didn't attach, skip the rest
            Debug::out(LOG_ERROR, "attachDevAsTranslated: failed to attach %s", mountPath.c_str());
            continue;
        }

        mapDeviceToHostPaths.insert(std::pair<std::string, std::string>(devName, mountPath) );  // store it to multimap
    }
}


