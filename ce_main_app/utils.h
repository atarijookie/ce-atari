#ifndef _UTILS_H_
#define _UTILS_H_

#include <signal.h>
#include <string>
#include <vector>

#include <stdint.h>

extern "C" volatile sig_atomic_t sigintReceived;

#ifndef MIN
    #define MIN(x, y)   (((x) < (y)) ? (x) : (y))
#endif

#ifndef MAX
    #define MAX(x, y)   (((x) > (y)) ? (x) : (y))
#endif

class Utils {
public:
    static uint32_t getCurrentMs(void);
    static uint32_t getEndTime(uint32_t offsetFromNow);
    static void  sleepMs(uint32_t ms);

    static void attributesHostToAtari(bool isReadOnly, bool isDir, uint8_t &attrAtari);
    static void fileDateTimeToHostTime(uint16_t atariDate, uint16_t atariTime, struct tm *ptm);
    static uint16_t fileTimeToAtariTime(struct tm *ptm);
    static uint16_t fileTimeToAtariDate(struct tm *ptm);

    static void mergeHostPaths(std::string &dest, const std::string &tail);                 // this modifies dest
    static std::string mergeHostPaths2(const std::string& head, const std::string& tail);   // this doesn't modify head
    static std::string mergeHostPaths3(const std::string& head, const char* tail);
    static void splitFilenameFromPath(const std::string &pathAndFile, std::string &path, std::string &file);
    static void splitFilenameFromExt(const std::string &filenameAndExt, std::string &filename, std::string &ext);
    static void splitToTwoByDelim(const std::string &input, std::string &beforeDelim, std::string &afterDelim, char delim);
    static void mergeFilenameAndExtension(const std::string& shortFn, const std::string& shortExt, bool extendWithSpaces, std::string& merged);
    static void extendWithSpaces(const char *normalFname, char *extendedFn);

    static void resetHansAndFranz(void);
    static void resetHans(void);
    static void resetFranz(void);

    static bool copyFile(std::string &src, std::string &dst);
    static bool copyFile(FILE *from, std::string &dst);
    static int  mkpath(const char *dir, int mode);

    static void SWAPWORD(uint16_t &w);
    static uint16_t SWAPWORD2(uint16_t w);

    static void getIpAdds(uint8_t *bfrIPs, uint8_t *bfrMasks=NULL);

    static void forceSync(void);

    static uint16_t  getWord(uint8_t *bfr);
    static uint32_t getDword(uint8_t *bfr);
    static uint32_t get24bits(uint8_t *bfr);

    static void storeWord(uint8_t *bfr, uint16_t val);
    static void storeDword(uint8_t *bfr, uint32_t val);

    static void createTimezoneString(char *str);
    static void setTimezoneVariable_inProfileScript(void);
    static void setTimezoneVariable_inThisContext(void);

    static std::string getDeviceLabel(const std::string & devicePath);

    static void splitString(const std::string &s, char delim, std::vector<std::string> &elems);

    static bool unZIPfloppyImageAndReturnFirstImage(const char *inZipFilePath, std::string &outImageFilePath);
    static const char *getExtension(const char *fileName);
    static bool isZIPfile(const char *fileName);
    static void createPathWithOtherExtension(std::string &inPathWithOriginalExt, const char *otherExtension, std::string &outPathWithOtherExtension);
    static bool fileExists(std::string &path);
    static bool fileExists(const char *path);
    static bool dirExists(std::string& path);
    static bool devExists(std::string& path);
    static void toUpperCaseString(std::string &st);

    static int bcdToInt(int bcd);

    static std::string dotEnvValue(std::string key, const char* defValue=NULL);
    static void loadDotEnv(void);
    static bool loadDotEnvFrom(const char* path);
    static int  dotEnvSubstituteVars(void);

    static void intToFile(int value, const char* filePath);                 // int to text file
    static void intToFileFromEnv(int value, const char* envKeyForFileName); // int to text file specified in .env
    static void textToFile(const char* text, const char* filePath);         // text to text file
    static void textToFileFromEnv(const char* text, const char* envKeyForFileName); // text to text file specified in .env

    static void textFromFile(char* bfr, uint32_t bfrSize, const char* filePath, bool trimTrailing=true); // get one line from text file
    static void trimTrail(char *bfr);

    static void screenShotVblEnabled(bool enabled);

    static void sendToMounter(const std::string& jsonString);
private:
    static bool copyFileByHandles(FILE *from, FILE *to);
    static void getDefaultValueFromVarName(std::string& varName, std::string& defValue, const std::string& delim);
};

#endif
