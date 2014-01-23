#ifndef _UTILS_H_
#define _UTILS_H_

#include <signal.h>
#include <string>

#include "datatypes.h"

extern "C" volatile sig_atomic_t sigintReceived;
#define MIN(x, y)	(((x) < (y)) ? (x) : (y))

class Utils {
public:
	static DWORD getCurrentMs(void);
	static DWORD getEndTime(DWORD offsetFromNow);
	static void  sleepMs(DWORD ms);
	
	static void attributesHostToAtari(bool isReadOnly, bool isDir, BYTE &attrAtari);
	static void fileDateTimeToHostTime(WORD atariDate, WORD atariTime, struct tm *ptm);
	static WORD fileTimeToAtariTime(struct tm *ptm);
	static WORD fileTimeToAtariDate(struct tm *ptm);
	
	static void mergeHostPaths(std::string &dest, std::string &tail);
	static void splitFilenameFromPath(std::string &pathAndFile, std::string &path, std::string &file);
};

#endif

