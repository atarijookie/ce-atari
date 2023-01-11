// vim: expandtab shiftwidth=4 tabstop=4
#ifndef _CONFIGSTREAM_H_
#define _CONFIGSTREAM_H_

#include <stdio.h>

#include "../settingsreloadproxy.h"
#include "../version.h"

// commands for HOSTMOD_CONFIG
#define CFG_CMD_IDENTIFY            0
#define CFG_CMD_KEYDOWN             1
#define CFG_CMD_SET_RESOLUTION      2
#define CFG_CMD_UPDATING_QUERY      3
#define CFG_CMD_REFRESH             0xfe
#define CFG_CMD_GO_HOME             0xff

#define CFG_CMD_SET_CFGVALUE        5

#define CFG_CMD_LINUXCONSOLE_GETSTREAM  10

#define CFG_CMD_GET_APP_NAMES       20
#define CFG_CMD_SET_APP_INDEX       21

// two values of a last byte of LINUXCONSOLE stream - more data, or no more data
#define LINUXCONSOLE_NO_MORE_DATA   0x00
#define LINUXCONSOLE_GET_MORE_DATA  0xda

// types for CFG_CMD_SET_CFGVALUE
#define CFGVALUE_TYPE_STRING    1
#define CFGVALUE_TYPE_INT       2
#define CFGVALUE_TYPE_BOOL      3
#define CFGVALUE_TYPE_ST_PATH   4   // to be translated

class AcsiDataTrans;

#define ST_RESOLUTION_LOW       0
#define ST_RESOLUTION_MID       1
#define ST_RESOLUTION_HIGH      2

#define READ_BUFFER_SIZE    (5 * 1024)

class ConsoleAppsStream
{
public:
    ConsoleAppsStream();
    ~ConsoleAppsStream();

    // functions which are called from the main loop
    void houseKeeping(uint32_t now);
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

    void readAppNames(void);
    void connectToAppIndex(int nextAppIndex);

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
