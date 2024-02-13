#ifndef __EXTENSION_H__
#define __EXTENSION_H__

#include <string>
#include <stdint.h>
#include "functiontable.h"
#include "response.h"

// states that describe if extension is running
#define EXT_STATE_NOT_RUNNING   0
#define EXT_STATE_INSTALLING    1
#define EXT_STATE_STARTING      2
#define EXT_STATE_RUNNING       3

class Extension
{
public:
    // clear extension state
    void clear(void);

    // store supplied name and url to this extension
    void storeNameUrl(const char* name, const char* url);

    // returns true if supplied name matches this extension's stored name
    bool nameMatching(char* name);

    void touch(void);

    void getStartScriptPath(std::string& outScriptPath);
    void getStopScriptPath(std::string& outScriptPath);
    void getScriptPath(std::string& outScriptPath, const char* scriptFile);
    void getInstallPath(std::string& outPath);
    static void installPath(char* extName, std::string& outPath);

    char name[32];                  // name of the extension
    char url[256];                  // url of the extension source

    uint8_t state;                  // one of the EXT_STATE_* values
    uint32_t lastAccessTime;        // when was the last time that ST wanted something from this extension
    char outSocketPath[256];

    FunctionTable functionTable;
    Response response;
};

#endif
