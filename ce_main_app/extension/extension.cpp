#include <string.h>
#include <stdint.h>
#include "extension.h"
#include "../utils.h"
#include "../debug.h"

void Extension::clear(void)
{
    storeNameUrl("", "");
    state = EXT_STATE_NOT_RUNNING;
    lastAccessTime = 0;
    memset(outSocketPath, 0, sizeof(outSocketPath));

    functionTable.clear();
    response.clear();
}

// store supplied name and url to this extension
void Extension::storeNameUrl(const char* name, const char* url)
{
    memset(this->name, 0, sizeof(this->name));
    memset(this->url, 0, sizeof(this->url));

    strncpy(this->name, name, sizeof(this->name) - 1);
    strncpy(this->url, url, sizeof(this->url) - 1);
}

// returns true if supplied name matches this extension's stored name
bool Extension::nameMatching(char* name)
{
    return (strncmp(this->name, name, sizeof(this->name) - 1) == 0);
}

void Extension::touch(void)
{
    lastAccessTime = Utils::getCurrentMs();
}

void Extension::getStartScriptPath(std::string& outScriptPath)
{
    getScriptPath(outScriptPath, "start.sh");
}

void Extension::getStopScriptPath(std::string& outScriptPath)
{
    getScriptPath(outScriptPath, "stop.sh");
}

void Extension::getScriptPath(std::string& outScriptPath, const char* scriptFile)
{
    getInstallPath(outScriptPath);                      // gets /ce/extensions/name
    std::string stopScriptName = scriptFile;
    Utils::mergeHostPaths(outScriptPath, stopScriptName);   // gets /ce/extensions/name/stop.sh
}

void Extension::getInstallPath(std::string& outPath)
{
    installPath(name, outPath);
}

void Extension::installPath(char* extName, std::string& outPath)
{
    std::string extNameStr = extName;
    outPath = Utils::dotEnvValue("EXT_INSTALL_DIR");
    Utils::mergeHostPaths(outPath, extNameStr);
}

void Extension::dumpToLog(void)
{
    Debug::out(LOG_DEBUG, "Extension::dumpToLog - name: '%s', url: '%s', state: %d, lastAccessTime: %d, outSocketPath: '%s'",
               name, url, state, lastAccessTime, outSocketPath);
}
