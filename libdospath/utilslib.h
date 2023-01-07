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

class UtilsLib {
    friend class TestClass;

public:
    static uint32_t getCurrentMs(void);

    static void toHostSeparators(std::string &path);
    static void removeTrailingSeparator(std::string& path);

    static void attributesHostToAtari(bool isReadOnly, bool isDir, uint8_t &attrAtari);
    static void fileDateTimeToHostTime(uint16_t atariDate, uint16_t atariTime, struct tm *ptm);
    static uint16_t fileTimeToAtariTime(struct tm *ptm);
    static uint16_t fileTimeToAtariDate(struct tm *ptm);

    static void mergeHostPaths(std::string &dest, const std::string &tail);
    static void splitFilenameFromPath(const std::string &pathAndFile, std::string &path, std::string &file);
    static void splitFilenameFromExt(const std::string &filenameAndExt, std::string &filename, std::string &ext);
    static void splitToTwoByDelim(const std::string &input, std::string &beforeDelim, std::string &afterDelim, char delim);

    static void splitString(const std::string &s, char delim, std::vector<std::string> &elems);
    static void joinStrings(std::vector<std::string> &elems, std::string& output, int count=-1);

    static const char *getExtension(const char *fileName);
    static bool fileExists(std::string &hostPath);

    static void out(int logLevel, const char *format, ...);
};

#endif

