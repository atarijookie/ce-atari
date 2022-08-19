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
    ConfigStream();
    ~ConfigStream();

    // functions which are called from the main loop
    void processCommand(uint8_t *cmd);
    void setAcsiDataTrans(AcsiDataTrans *dt);
    void setSettingsReloadProxy(SettingsReloadProxy *rp);

    uint32_t getLastCmdTimestamp() const {
        return lastCmdTime;
    }
private:
    int stScreenWidth;
    int gotoOffset;

    int appIndex;       // index of app to which we want to connect, e.g. 0 for /var/run/ce/app0.sock
    int appFd;          // file descriptor for socket connected to app

    AcsiDataTrans       *dataTrans;
    SettingsReloadProxy *reloadProxy;

    // private methods
    void onKeyDown(uint8_t key);
    int  getStream(uint8_t *bfr, int maxLen);

    uint32_t lastCmdTime;
   
    int  filterVT100(char *bfr, int cnt);
    void atariKeyToConsoleKey(uint8_t atariKey, char *bfr, int &cnt);

    void connectIfNeeded(void);
    int openSocket(void);
    void closeFd(int& fd);
    ssize_t sockRead(int& fdIn, char* bfr, int bfrLen);
    ssize_t sockWrite(int& fdOut, char* bfr, int count);
};

#endif
