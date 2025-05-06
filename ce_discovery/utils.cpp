// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
#include "utils.h"

#include <libgen.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/select.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <fcntl.h>

//--------
// following includes are here for the code for getting IP address of interfaces
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <linux/if_link.h>
//--------

#include "debug.h"
#include "global.h"

std::map<std::string, std::string> dotEnv;

uint32_t Utils::getCurrentMs(void)
{
    struct timespec tp;
    int res;

    res = clock_gettime(CLOCK_MONOTONIC, &tp);                  // get current time

    if(res != 0) {                                              // if failed, fail
        return 0;
    }

    uint32_t val = (tp.tv_sec * 1000) + (tp.tv_nsec / 1000000);    // convert to milli seconds
    return val;
}

std::string Utils::mergeHostPaths3(const std::string& head, const char* tail)
{
    std::string tailStr(tail);
    return Utils::mergeHostPaths2(head, tailStr);
}

std::string Utils::mergeHostPaths2(const std::string& head, const std::string& tail)
{
    // this method creates merged path, and it doesn't modifiy head:    rev_val = head + tail

    static std::string retVal;              // static string, which will be used as return value
    retVal = head;                          // copy of head into return value, so we won't modify head
    Utils::mergeHostPaths(retVal, tail);    // merge retVal (head) with tail
    return retVal;                          // return merged value
}

void Utils::mergeHostPaths(std::string &dest, const std::string &tail)
{
    // this method creates merged path, but it modifies dest:     dest = dest + tail

    if(dest.empty()) {      // if the 1st part is empty, then result is just the 2nd part
        dest = tail;
        return;
    }

    if(tail.empty()) {      // if the 2nd part is empty, don't do anything
        return;
    }

    bool endsWithSepar      = (dest[dest.length() - 1] == HOSTPATH_SEPAR_CHAR);
    bool startsWithSepar    = (tail[0] == HOSTPATH_SEPAR_CHAR);

    if(!endsWithSepar && !startsWithSepar){     // both don't have separator char? add it between them
        dest = dest + HOSTPATH_SEPAR_STRING + tail;
        return;
    }

    if(endsWithSepar && startsWithSepar) {      // both have separator char? remove one
        dest[dest.length() - 1] = 0;
        dest = dest + tail;
        return;
    }

    // in this case one of them has separator, so just merge them together
    dest = dest + tail;
}


uint16_t Utils::getWord(uint8_t *bfr)
{
    uint16_t val = 0;

    val = bfr[0];       // get hi
    val = val << 8;

    val |= bfr[1];      // get lo

    return val;
}

uint32_t Utils::getDword(uint8_t *bfr)
{
    uint32_t val = 0;

    val = bfr[0];       // get hi
    val = val << 8;

    val |= bfr[1];      // get mid hi
    val = val << 8;

    val |= bfr[2];      // get mid lo
    val = val << 8;

    val |= bfr[3];      // get lo

    return val;
}

uint32_t Utils::get24bits(uint8_t *bfr)
{
    uint32_t val = 0;

    val  = bfr[0];       // get hi
    val  = val << 8;

    val |= bfr[1];      // get mid
    val  = val << 8;

    val |= bfr[2];      // get lo

    return val;
}

void Utils::storeWord(uint8_t *bfr, uint16_t val)
{
    bfr[0] = val >> 8;  // store hi
    bfr[1] = val;       // store lo
}

void Utils::storeDword(uint8_t *bfr, uint32_t val)
{
    bfr[0] = val >> 24; // store hi
    bfr[1] = val >> 16; // store mid hi
    bfr[2] = val >>  8; // store mid lo
    bfr[3] = val;       // store lo
}

void Utils::store24bits(uint8_t *bfr, uint32_t val)
{
    bfr[0] = val >> 16;
    bfr[1] = val >>  8;
    bfr[2] = val;
}

void Utils::loadDotEnv(void)
{
    if(loadDotEnvFrom("./.env")) {      // if something was loaded, do the vars subtitution
        dotEnvSubstituteVars();
    }
}

std::string Utils::dotEnvValue(std::string key, const char* defValue)
{
    /* get value from dotEnv map for specified key */

    try {
        std::string& value = dotEnv.at(key);             // get value and return it (if found in map)
        return value;
    }
    catch (const std::out_of_range&) {
        Debug::out(LOG_DEBUG, "Utils::dotEnvValue - no value for key '%s' !", key.c_str());
    }

    // if got here, the value wasn't found in map, but it still could be a real env var, so try getting it
    char* envVar = getenv(key.c_str());

    if(envVar) {    // some real env var was found with this name?
        static std::string retValueFromEnv;
        retValueFromEnv = envVar;
        Debug::out(LOG_DEBUG, "Utils::dotEnvValue - ...but found env var '%s' with value '%s'", key.c_str(), retValueFromEnv.c_str());
        return retValueFromEnv;
    }

    // if value not found and default value was provided, use it; otherwise return empty string
    std::string defValueStr = defValue ? std::string(defValue) : std::string("");
    return defValueStr;
}

void Utils::getDefaultValueFromVarName(std::string& varName, std::string& defValue, const std::string& delim)
{
    std::size_t varDef = varName.find(delim);    // check if this var name has also default value specified

    //Debug::out(LOG_DEBUG, "Utils::getDefaultValueFromVarName - varName '%s'", varName.c_str());

    if(varDef != std::string::npos) {           // if this variable name has also default value specified
        std::string newVarName = varName.substr(0, varDef); // get just var name without default value
        defValue = varName.substr(varDef + delim.length()); // get just the default value
        //Debug::out(LOG_DEBUG, "Utils::getDefaultValueFromVarName - varName with default: '%s', varName '%s', defValue: '%s'", varName.c_str(), newVarName.c_str(), defValue.c_str());
        varName = newVarName;                               // use the new var name
    }
}

int Utils::dotEnvSubstituteVars(void)
{
    /* go through the current dotEnv values and replace vars with values */

    Debug::out(LOG_DEBUG, "Utils::dotEnvSubstituteVars starting");

    int found = 0;

    std::map<std::string, std::string>::iterator it = dotEnv.begin();
    while (it != dotEnv.end())                          // go through all map values
    {
        std::string key = it->first;
        std::string value = it->second;

        std::size_t varStart = value.find("${");       // var start tag
        std::size_t varEnd = value.find("}");          // var end tag

        if(varStart != std::string::npos) {                 // start tag was found?
            std::string varName = value.substr(varStart + 2, varEnd - varStart - 2);    // get just var name

            std::string defValue;
            Utils::getDefaultValueFromVarName(varName, defValue, std::string(":-"));        // try the longer first
            Utils::getDefaultValueFromVarName(varName, defValue, std::string("-"));         // then shorter next

            std::size_t varDef = varName.find(":-");    // check if this var name has also default value specified

            //Debug::out(LOG_DEBUG, "Utils::dotEnvSubstituteVars - varName '%s'", varName.c_str());

            if(varDef != std::string::npos) {           // if this variable name has also default value specified
                std::string newVarName = varName.substr(0, varDef); // get just var name without default value
                defValue = varName.substr(varDef + 2);              // get just the default value
                //Debug::out(LOG_DEBUG, "Utils::dotEnvSubstituteVars - varName with default: '%s', varName '%s', defValue: '%s'", varName.c_str(), newVarName.c_str(), defValue.c_str());
                varName = newVarName;                               // use the new var name
            }

            std::string varValue = Utils::dotEnvValue(varName, defValue.c_str());   // get variable value with possible default value
            //Debug::out(LOG_DEBUG, "Utils::dotEnvSubstituteVars - for var '%s' found value '%s'", varName.c_str(), varValue.c_str());

            value.replace(varStart, varEnd - varStart + 1, varValue);   // replace variable in original value
            //Debug::out(LOG_DEBUG, "Utils::dotEnvSubstituteVars - value after replacing var: '%s'", value.c_str());

            dotEnv[key] = value;        // store new value back to map
            found++;
        }

        ++it;
    }

    return found;
}

bool Utils::loadDotEnvFrom(const char* path)
{
    /* try to load .env file from the specified path */

    FILE *f = fopen(path, "rt");        // try to open file

    if(!f) {
        Debug::out(LOG_ERROR, "Utils::loadDotEnv - failed to open file %s", path);
        return false;
    }

    char line[1024];

    while(true) {                           // go through file line by line
        if(feof(f)) {
            break;
        }

        int eqlPos = -1;                    // where the '=' is
        memset(line, 0, sizeof(line));
        fgets(line, sizeof(line) - 1, f);   // get one line, including '\n' symbol

        // first loop - remove new line chars, and everything after comment symbol
        int len = strlen(line);     // get length of line
        for(int i=0; i<len; i++) {
            if(line[i] == '\n' || line[i] == '\r' || line[i] == '#') {     // remove EOL, RET, and if it's a start of comment, ignore the rest of line
                line[i] = 0;        // string ends here now
                break;
            }

            if(line[i] == '=') {    // found equal (=) sign? store position
                eqlPos = i;
            }
        }

        // second loop - trim trailing white spaces
        len = strlen(line);         // get length of line
        for(int i=(len-1); i>=0; i--) {
            if(line[i] == ' ' || line[i] == '\t') {     // space or tab? trim
                line[i] = 0;
            }
        }

        // check if something remained after previous changes to line
        len = strlen(line);         // get length of line
        if(len < 1 || eqlPos < 0) { // line empty or no equal sign there? skip it
            continue;
        }

        line[eqlPos] = 0;           // split the string on the equal sign

        std::string key, value;
        key = line;                 // key   is on [0 : eqlPos-1]
        value = line + eqlPos + 1;  // value is on [eqlPos+1 : ...]
        //Debug::out(LOG_DEBUG, "Utils::loadDotEnv - found %s -> %s", key.c_str(), value.c_str());

        dotEnv[key] = value;        // store to map
    }

    fclose(f);
    return true;
}

void Utils::intToFileFromEnv(int value, const char* envKeyForFileName)
{
    std::string fileNameFromEnv = Utils::dotEnvValue(envKeyForFileName);    // fetch filename from env by key
    Utils::intToFile(value, fileNameFromEnv.c_str());                       // int to filename
}

void Utils::intToFile(int value, const char* filePath)
{
    char bfr[128];
    int lastIndex = sizeof(bfr) - 1;
    bfr[lastIndex] = 0;                     // zero terminate buffer

    snprintf(bfr, lastIndex, "%d", value);  // integer to string
    Utils::textToFile(bfr, filePath);       // string to file
}

void Utils::textToFileFromEnv(const char* text, const char* envKeyForFileName)
{
    std::string fileNameFromEnv = Utils::dotEnvValue(envKeyForFileName);    // fetch filename from env by key
    Utils::textToFile(text, fileNameFromEnv.c_str());                       // text to filename
}

void Utils::textToFile(const char* text, const char* filePath)
{
    if(!filePath || strlen(filePath) < 1) {     // null path or empty string path? quit
        return;
    }

    FILE *f = fopen(filePath, "wt");    // open file

    if(!f) {                            // could not open file? quit
        return;
    }

    fputs(text, f);                     // write text to file
    fclose(f);                          // close file
}
