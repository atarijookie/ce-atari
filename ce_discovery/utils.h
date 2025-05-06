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

#define HOSTPATH_SEPAR_STRING       "/"
#define HOSTPATH_SEPAR_CHAR         '/'
#define ATARIPATH_SEPAR_CHAR        '\\'

class Utils {
public:
    static uint32_t getCurrentMs(void);

    static void mergeHostPaths(std::string &dest, const std::string &tail);                 // this modifies dest
    static std::string mergeHostPaths2(const std::string& head, const std::string& tail);   // this doesn't modify head
    static std::string mergeHostPaths3(const std::string& head, const char* tail);

    static uint16_t  getWord(uint8_t *bfr);
    static uint32_t getDword(uint8_t *bfr);
    static uint32_t get24bits(uint8_t *bfr);

    static void storeWord(uint8_t *bfr, uint16_t val);
    static void storeDword(uint8_t *bfr, uint32_t val);
    static void store24bits(uint8_t *bfr, uint32_t val);

    static std::string dotEnvValue(std::string key, const char* defValue=NULL);
    static void loadDotEnv(void);
    static bool loadDotEnvFrom(const char* path);
    static int  dotEnvSubstituteVars(void);

    static void intToFile(int value, const char* filePath);                 // int to text file
    static void intToFileFromEnv(int value, const char* envKeyForFileName); // int to text file specified in .env
    static void textToFile(const char* text, const char* filePath);         // text to text file
    static void textToFileFromEnv(const char* text, const char* envKeyForFileName); // text to text file specified in .env

private:
    static void getDefaultValueFromVarName(std::string& varName, std::string& defValue, const std::string& delim);
};

#endif
