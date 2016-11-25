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
	
	static void mergeHostPaths(std::string &dest, const std::string &tail);
	static void splitFilenameFromPath(const std::string &pathAndFile, std::string &path, std::string &file);

	static void resetHansAndFranz(void);
    static void resetHans(void);
    static void resetFranz(void);

    static bool copyFile(std::string &src, std::string &dst);
    static bool copyFile(FILE *from, std::string &dst);

    static void SWAPWORD(WORD &w);
    static WORD SWAPWORD2(WORD w);

    static void getIpAdds(BYTE *bfr);
    
    static void forceSync(void);

    static WORD  getWord(BYTE *bfr);
    static DWORD getDword(BYTE *bfr);
    static DWORD get24bits(BYTE *bfr);

    static void storeWord(BYTE *bfr, WORD val);
    static void storeDword(BYTE *bfr, DWORD val);

    static void createTimezoneString(char *str);
    static void setTimezoneVariable_inProfileScript(void);
    static void setTimezoneVariable_inThisContext(void);

    static std::string getDeviceLabel(const std::string & devicePath);
private:
    static bool copyFileByHandles(FILE *from, FILE *to);

};

#endif

