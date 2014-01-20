#include <stdio.h>
#include <string.h>

#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define DISK_LINKS_PATH		"/dev/disk/by-id"
#define PATH_BUFF_SIZE		1024

#include "translated/translatedhelper.h"
#include "devfinder.h"
#include "utils.h"

DevFinder::DevFinder()
{
	devChHandler = NULL;
}

void DevFinder::setDevChangesHandler(DevChangesHandler *devChHand)
{
	devChHandler = devChHand;
}

void DevFinder::lookForDevChanges(void)
{
	char linkBuf[PATH_BUFF_SIZE];
	char devBuf[PATH_BUFF_SIZE];
	
	someDevChanged = false;
	
	DIR *dir = opendir(DISK_LINKS_PATH);							// try to open the dir
	
    if(dir == NULL) {                                 				// not found?
        return;
    }

	clearFoundFlags();												// mark all items in map as not found yet
	
	while(1) {                                                  	// while there are more files, store them
		struct dirent *de = readdir(dir);							// read the next directory entry
	
		if(de == NULL) {											// no more entries?
			break;
		}

		if(de->d_type != DT_LNK) {									// if it's not a link, skip it
			continue;
		}

		memset(linkBuf,	0, PATH_BUFF_SIZE);
		memset(devBuf,	0, PATH_BUFF_SIZE);
		
		strcpy(linkBuf, DISK_LINKS_PATH);
		strcat(linkBuf, HOSTPATH_SEPAR_STRING);
		strcat(linkBuf, de->d_name);
		
		int ires = readlink(linkBuf, devBuf, PATH_BUFF_SIZE);		// try to resolve the filename from the link
		if(ires == -1) {
			continue;
		}
		
		std::string pathAndFile = devBuf;
		std::string path, file;
		
		Utils::splitFilenameFromPath(pathAndFile, path, file);				// get only file name, skip the path
		
		if(file.find("mmcblk") != std::string::npos) {				// and if it's SD card (the one from which RPi boots), skip it
			continue;
		}
		
		cutBeforeFirstNumber(file);									// cut before the 1st number (e.g. sda1 -> sda)
		processFoundDev(file);										// and do something with that file
    }

	closedir(dir);	
	
	findAndSignalDettached();										// now go through the mapDevToFound and see what disappeared
	
//	if(!someDevChanged) {
//		printf("no change\n");
//	}
}

void DevFinder::getDevPartitions(std::string devName, std::list<std::string> &partitions)
{
	partitions.erase(partitions.begin(), partitions.end());		// erease list content
	
	char linkBuf[PATH_BUFF_SIZE];
	char devBuf[PATH_BUFF_SIZE];
	
	DIR *dir = opendir(DISK_LINKS_PATH);							// try to open the dir
	
    if(dir == NULL) {                                 				// not found?
        return;
    }

	while(1) {                                                  	// while there are more files, store them
		struct dirent *de = readdir(dir);							// read the next directory entry
	
		if(de == NULL) {											// no more entries?
			break;
		}

		if(de->d_type != DT_LNK) {									// if it's not a link, skip it
			continue;
		}

		memset(linkBuf,	0, PATH_BUFF_SIZE);
		memset(devBuf,	0, PATH_BUFF_SIZE);
		
		strcpy(linkBuf, DISK_LINKS_PATH);
		strcat(linkBuf, HOSTPATH_SEPAR_STRING);
		strcat(linkBuf, de->d_name);
		
		int ires = readlink(linkBuf, devBuf, PATH_BUFF_SIZE);		// try to resolve the filename from the link
		if(ires == -1) {
			continue;
		}
		
		std::string pathAndFile = devBuf;
		std::string path, file;
		
		Utils::splitFilenameFromPath(pathAndFile, path, file);		// get only file name, skip the path
		
		if(file.length() <= devName.length()) {						// if what we've found is shorter or equaly long as what we are looking for, skip it
			continue;
		}
		
		if(file.find(devName) == 0) {								// if the file name starts with the device
			partitions.push_back(file);
		}
    }

	closedir(dir);	
	
	partitions.sort();												// sort them alphabetically
}

void DevFinder::processFoundDev(std::string file)
{
	std::map<std::string, bool>::iterator it;
	it = mapDevToFound.find(file);									// try to find the device in the map
	
	if(it == mapDevToFound.end()) {									// don't have this device? add it, signal it
		mapDevToFound.insert( std::pair<std::string, bool>(file, true) );
		someDevChanged = true;
		
		bool atariDrive = isAtariDrive(file);
		
		printf("device attached: %s, is atari drive: %d\n", (char *) file.c_str(), atariDrive);		// write out

		if(devChHandler != NULL) {									// if got handler, notify him
			devChHandler->onDevAttached(file, atariDrive);
		}
	} else {														// have this device? just mark it as found again
		it->second = true;
	}	
}

void DevFinder::cutBeforeFirstNumber(std::string &filename)
{
	int i;
	
	for(i=0; i<filename.length(); i++) {						// go through the file name looking for a number
		if(filename[i] >= '0' && filename[i] <= '9') {			// if this is a number, cut it here
			filename.resize(i);
			return;
		}	
	}
}

void DevFinder::clearFoundFlags(void)
{
	std::map<std::string, bool>::iterator it;							// set all found flags to false

    for(it = mapDevToFound.begin(); it != mapDevToFound.end(); ++it) {
		it->second = false;
	}
}

void DevFinder::findAndSignalDettached(void)
{
	std::map<std::string, bool>::iterator it, del;							// set all found flags to false

	del = mapDevToFound.end();
	
    for(it = mapDevToFound.begin(); it != mapDevToFound.end(); ++it) {		// go through the map and look for false found flag
		if(del != mapDevToFound.end()) {									// if we should erase something, erase it
			mapDevToFound.erase(del);
			del = mapDevToFound.end();
		}
		
		if(it->second == false) {											// not found? it was detached
			del = it;														// delete dev in next step
			someDevChanged = true;
			
			printf("device detached: %s\n", (char *) it->first.c_str());

			if(devChHandler != NULL) {										// if got handler, notify him
				devChHandler->onDevDetached(it->first);
			}
		}
	}
	
	if(del != mapDevToFound.end()) {										// if we should erase something, erase it
		mapDevToFound.erase(del);
	}
}

bool DevFinder::isAtariDrive(std::string file)
{
	if(geteuid() != 0) {
		printf("Warning! Not running as root, won't be able to check dev partitions types!\n");
	}
	
	std::string fullPath = "/dev/" + file;
	char bfr[512];
	
	int fdes = open((char *) fullPath.c_str(), O_RDONLY);					// try to open the device

	if (fdes < 0) {															// failed to open?
		return false;
	}
	
	int cnt = read(fdes, bfr, 512);											// try to read sector #0
	close(fdes);															// close device
	
	if(cnt < 512) {															// failed?
		return false;
	}
	
	if(isAtariPartitionType(&bfr[0x1c6])) {									// check partition header
		return true;
	}
	
	if(isAtariPartitionType(&bfr[0x1d2])) {									// check partition header
		return true;
	}
	
	if(isAtariPartitionType(&bfr[0x1de])) {									// check partition header
		return true;
	}
	
	if(isAtariPartitionType(&bfr[0x1ea])) {									// check partition header
		return true;
	}

	return false;															// no atari partition found 
}

bool DevFinder::isAtariPartitionType(char *bfr) 
{
	if((bfr[0] & 0x01) == 0) {				// partition exists flag not set?
		return false;
	}

	if(strncmp(bfr + 1, "GEM", 3) == 0) {	// it's a GEM partition?
		return true;
	}

	if(strncmp(bfr + 1, "BGM", 3) == 0) {	// it's a BGM partition?
		return true;
	}

	if(strncmp(bfr + 1, "XGM", 3) == 0) {	// it's a XGM partition?
		return true;
	}
	
	return false;							// no atari partition found
}


