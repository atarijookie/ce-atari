#ifndef _DEVFINDER_H_
#define _DEVFINDER_H_

#include <string>
#include <iostream>
#include <map>
#include <list>

#include <stdio.h>
#include <string.h>

class DevFinder {
public:
	DevFinder();

	void lookForDevChanges(void);
	void getDevPartitions(std::string devName, std::list<std::string> &partitions);

	void clearMap(void);						// called to make all the devices appear as new
	
private:
	std::map<std::string, bool>  mapDevToFound;
	bool someDevChanged;
    std::multimap<std::string, std::string> mapDeviceToHostPaths;

	void cutBeforeFirstNumber(std::string &filename);

	void clearFoundFlags(void);
	void processFoundDev(std::string file);
	void findAndSignalDettached(void);
	
	bool isAtariDrive(std::string file);
	bool isAtariPartitionType(char *bfr);
    
    void onDevAttached(std::string devName, bool isAtariDrive);
    void onDevDetached(std::string devName);
    void attachDevAsTranslated(std::string devName);
};
	
#endif

