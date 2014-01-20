#ifndef _DEVFINDER_H_
#define _DEVFINDER_H_

#include <string>
#include <iostream>
#include <map>

#include <stdio.h>
#include <string.h>

#include "devchangeshandler.h"

class DevFinder {
public:
	DevFinder();

	void lookForDevChanges(void);
	void setDevChangesHandler(DevChangesHandler *devChHand);
	
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

