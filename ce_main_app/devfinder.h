#ifndef _DEVFINDER_H_
#define _DEVFINDER_H_

#include <string>
#include <iostream>
#include <map>
#include <list>

#include <stdio.h>
#include <string.h>

#include "devchangeshandler.h"

class DevFinder {
public:
	DevFinder();

	void setDevChangesHandler(DevChangesHandler *devChHand);

	void lookForDevChanges(void);
	void getDevPartitions(std::string devName, std::list<std::string> &partitions);

	void clearMap(void);						// called to make all the devices appear as new
	
private:
	DevChangesHandler *devChHandler;

	std::map<std::string, bool>  mapDevToFound;
	bool someDevChanged;

	void cutBeforeFirstNumber(std::string &filename);

	void clearFoundFlags(void);
	void processFoundDev(std::string file);
	void findAndSignalDettached(void);
	
	bool isAtariDrive(std::string file);
	bool isAtariPartitionType(char *bfr);
};
	
#endif

