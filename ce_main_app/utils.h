#ifndef _UTILS_H_
#define _UTILS_H_

#include <signal.h>
#include <string>
#include <vector>

#include "datatypes.h"

extern "C" volatile sig_atomic_t sigintReceived;

#ifndef MIN
    #define MIN(x, y)   (((x) < (y)) ? (x) : (y))
#endif

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
    static void splitFilenameFromExt(const std::string &filenameAndExt, std::string &filename, std::string &ext);

	static void resetHansAndFranz(void);
    static void resetHans(void);
    static void resetFranz(void);

    static bool copyFile(std::string &src, std::string &dst);
    static bool copyFile(FILE *from, std::string &dst);
    static int  mkpath(const char *dir, int mode);

    static void SWAPWORD(WORD &w);
    static WORD SWAPWORD2(WORD w);

    static void getIpAdds(BYTE *bfrIPs, BYTE *bfrMasks=NULL);

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

    static void splitString(const std::string &s, char delim, std::vector<std::string> &elems);

    static const char*IFintToString(int IFtype);
    static       void IFintToStringFormatted(int IFtype, char *outBuffer, const char *format);

    static BYTE bcdToDec(BYTE bcd);
    static BYTE decToBCD(BYTE dec);

    static bool unZIPfloppyImageAndReturnFirstImage(const char *inZipFilePath, std::string &outImageFilePath);
    static const char *getExtension(const char *fileName);
    static bool isZIPfile(const char *fileName);
    static void createPathWithOtherExtension(std::string &inPathWithOriginalExt, const char *otherExtension, std::string &outPathWithOtherExtension);
    static bool fileExists(std::string &hostPath);

private:
    static bool copyFileByHandles(FILE *from, FILE *to);

};

#endif

