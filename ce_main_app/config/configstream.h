// vim: expandtab shiftwidth=4 tabstop=4
#ifndef _CONFIGSTREAM_H_
#define _CONFIGSTREAM_H_

#include <stdio.h>

#include "../settingsreloadproxy.h"
#include "../version.h"

#define CONFIG_TEXT_FILE        "/tmp/ce_config.txt"

#define CONFIGSTREAM_ON_ATARI           0
#define CONFIGSTREAM_IN_LINUX_CONSOLE   1
#define CONFIGSTREAM_THROUGH_WEB        2

#define FAILED_UPDATE_CHECK_TIME    (30 * 1000)

class AcsiDataTrans;

#define ST_RESOLUTION_LOW       0
#define ST_RESOLUTION_MID       1
#define ST_RESOLUTION_HIGH      2

#define READ_BUFFER_SIZE    (5 * 1024)

class ConfigStream
{
public:
    ConfigStream(int whereItWillBeShown);
    ~ConfigStream();

    // functions which are called from the main loop
    void processCommand(uint8_t *cmd, int writeToFd=-1);
    void setAcsiDataTrans(AcsiDataTrans *dt);
    void setSettingsReloadProxy(SettingsReloadProxy *rp);

    uint32_t getLastCmdTimestamp() const {
        return lastCmdTime;
    }
private:
    int stScreenWidth;
    int gotoOffset;

    AcsiDataTrans       *dataTrans;
    SettingsReloadProxy *reloadProxy;

    // private methods
    void onKeyDown(uint8_t key);
    int  getStream(bool homeScreen, uint8_t *bfr, int maxLen);

    uint32_t lastCmdTime;

    //-------------
    // remote console stuff
    void linuxConsole_KeyDown(uint8_t atariKey);
    int  linuxConsole_getStream(uint8_t *bfr, int maxLen);
    
    int  filterVT100(char *bfr, int cnt);
    void atariKeyToConsoleKey(uint8_t atariKey, char *bfr, int &cnt);
    // Set values
    //void onSetCfgValue(void);
};

#endif
