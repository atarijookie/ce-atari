// vim: expandtab shiftwidth=4 tabstop=4
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <queue>
#include <pty.h>
#include <sys/file.h>
#include <errno.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>

#include "../global.h"
#include "../native/scsi_defs.h"
#include "../acsidatatrans.h"
#include "../update.h"
#include "../translated/translateddisk.h"

#include "../settings.h"
#include "../utils.h"
#include "keys.h"
#include "consoleappsstream.h"
#include "../debug.h"

uint8_t isUpdateStartingFlag = 0;

ConsoleAppsStream::ConsoleAppsStream()
{
    dataTrans   = NULL;
    reloadProxy = NULL;

    lastCmdTime = 0;
    appIndex = 0;       // start with app0
    appFd = -1;         // not connected

    hasConnected = false;   // has not connected yet
}

ConsoleAppsStream::~ConsoleAppsStream()
{

}

void ConsoleAppsStream::setAcsiDataTrans(AcsiDataTrans *dt)
{
    dataTrans = dt;
}

void ConsoleAppsStream::setSettingsReloadProxy(SettingsReloadProxy *rp)
{
    reloadProxy = rp;
}

void ConsoleAppsStream::houseKeeping(uint32_t now)
{
    if((now - lastCmdTime) >= 3000) {       // no commands for apps for some time? disconnect app socket to let the app close
        closeFd(appFd);
    }
}

void ConsoleAppsStream::processCommand(uint8_t *cmd)
{
    static uint8_t readBuffer[READ_BUFFER_SIZE];
    int streamCount;

    if(cmd[1] != 'C' || cmd[2] != 'E' || cmd[3] != HOSTMOD_CONFIG) {        // not for us?
        return;
    }

    dataTrans->clear();                 // clean data transporter before handling

    lastCmdTime = Utils::getCurrentMs();

    connectIfNeeded();

    switch(cmd[4]) {
    case CFG_CMD_IDENTIFY:          // identify?
        dataTrans->addDataBfr("CosmosEx config console", 23, true);       // add identity string with padding
        dataTrans->setStatus(SCSI_ST_OK);
        break;

    case CFG_CMD_GO_HOME:       // deprecated, falls through to keydown
    case CFG_CMD_REFRESH:       // deprecated, falls through to keydown
        cmd[5] = 0;             // no pressed key, fall through

    case CFG_CMD_LINUXCONSOLE_GETSTREAM:                                    // this command is deprecated and now it's just the same as get stream / key down
    case CFG_CMD_KEYDOWN:
    {
        if(cmd[5] != 0) {                   // key was really pressed, send it
            char consoleKeys[10];
            int  consoleKeysLength = 0;
            atariKeyToConsoleKey(cmd[5], consoleKeys, consoleKeysLength);   // convert key from Atari pseudo key to console key
            sockWrite(appFd, consoleKeys, consoleKeysLength);               // send to app
        }

        if(hasConnected) {                              // if this has connected to new / other app, we should clear ST screen
            hasConnected = false;                       // don't clear screen until next connect
            dataTrans->addDataBfr("\033E", 2, false);   // add clear screen VT52 command, don't pad yet
        }

        int readCount = MIN(READ_BUFFER_SIZE, (6 * 512));                   // less from: size of buffer vs size which ST will try to get
        streamCount = getStream(readBuffer, readCount);                     // then get current screen stream
        streamCount = convertVT100toVT52((char *) readBuffer, streamCount); // VT100 to VT52

        dataTrans->addDataBfr(readBuffer, streamCount, true);               // add data and status, with padding to multiple of 16 bytes
        dataTrans->setStatus(SCSI_ST_OK);

        Debug::out(LOG_DEBUG, "handleConsoleAppsStream -- CFG_CMD_KEYDOWN -- %d bytes", streamCount);
        break;
    }

    case CFG_CMD_SET_RESOLUTION:
        char bfr[3];
        memset(bfr, 0, sizeof(bfr));

        switch(cmd[5]) {
            case ST_RESOLUTION_LOW:
                bfr[0] = 0xfa; break;       // 40 cols

            case ST_RESOLUTION_MID:
            case ST_RESOLUTION_HIGH:
                bfr[0] = 0xfb; break;       // 80 cols
        }

        sockWrite(appFd, bfr, 1);           // send to app

        dataTrans->setStatus(SCSI_ST_OK);
        Debug::out(LOG_DEBUG, "handleConsoleAppsStream -- CFG_CMD_SET_RESOLUTION -- %d", cmd[5]);
        break;

    case CFG_CMD_SET_CFGVALUE:
        break;

    case CFG_CMD_UPDATING_QUERY:
    {
        uint8_t updateComponentsWithValidityNibble = 0xC0 | 0x0f;          // for now pretend all needs to be updated

        dataTrans->addDataByte(isUpdateStartingFlag);
        dataTrans->addDataByte(updateComponentsWithValidityNibble);
        dataTrans->padDataToMul16();

        dataTrans->setStatus(SCSI_ST_OK);

        Debug::out(LOG_DEBUG, "handleConsoleAppsStream -- CFG_CMD_UPDATING_QUERY -- isUpdateStartingFlag: %d, updateComponentsWithValidityNibble: %x", isUpdateStartingFlag, updateComponentsWithValidityNibble);
        break;
    }

    case CFG_CMD_GET_APP_NAMES:
        // read app names from file and send them to ST
        readAppNames();
        break;

    case CFG_CMD_SET_APP_INDEX:
        // switch to different app as asked by user
        connectToAppIndex(cmd[5]);
        break;

    default:                            // other cases: error
        dataTrans->setStatus(SCSI_ST_CHECK_CONDITION);
        break;
    }

    dataTrans->sendDataAndStatus();     // send all the stuff after handling, if we got any
}

void ConsoleAppsStream::connectToAppIndex(int nextAppIndex)
{
    uint8_t status = SCSI_ST_CHECK_CONDITION;   // default status to fail

    char sockPath[128];                     // construct filename for socket, e.g. /var/run/ce/app0.sock
    std::string sockPathFormat = Utils::dotEnvValue("APPS_VIA_SOCK_PATHS");     // fetch format for app-via-sock path to sock
    sprintf(sockPath, sockPathFormat.c_str(), nextAppIndex);                    // create path for nextAppIndex

    if (access(sockPath, F_OK) == 0) {      // socket path valid
        Debug::out(LOG_DEBUG, "handleConsoleAppsStream -- CFG_CMD_SET_APP_INDEX -- connecting to app %d - path: %s", nextAppIndex, sockPath);

        appIndex = nextAppIndex;            // store next socket index
        closeFd(appFd);                     // close current socket
        connectIfNeeded();                  // open next socket
        status = SCSI_ST_OK;                // success!
    } else {                                // socket path not found
        Debug::out(LOG_WARNING, "handleConsoleAppsStream -- CFG_CMD_SET_APP_INDEX -- app %d with path %s does not exist", nextAppIndex, sockPath);
    }

    dataTrans->setStatus(status);           // return status
}

void ConsoleAppsStream::readAppNames(void)
{
    std::string sockDescPathFormat = Utils::dotEnvValue("APPS_VIA_SOCK_DESCS");     // fetch format for app-via-sock path to description

    for(int i=0; i<9; i++) {
        char descPath[128];
        sprintf(descPath, sockDescPathFormat.c_str(), i);   // create path

        char line[40];
        memset(line, 0, sizeof(line));                      // clear line before reading
        sprintf(line, "[F%d] ", i + 1);                     // generate key to switch here, e.g. [F1]

        Utils::textFromFile(line + 5, sizeof(line) - 6, descPath);      // read in line from file

        if(line[5] == 0) {                                  // empty description? clear the '[Fx] ' part
            memset(line, 0, 5);
            Debug::out(LOG_DEBUG, "readAppNames - app: %d, not present", i);
        } else {                                            // got description? log it
            Debug::out(LOG_DEBUG, "readAppNames - app: %d, desc: %s", i, line);
        }

        dataTrans->addDataBfr(line, 40, false);             // add line to bfr
    }

    dataTrans->padDataToMul16();            // pad buffer
    dataTrans->setStatus(SCSI_ST_OK);       // success
}

int ConsoleAppsStream::getStream(uint8_t *bfr, int maxLen)
{
    memset(bfr, 0, maxLen);                             // clear the buffer

    int totalCnt = 0;
    int res, bytesAvailable;
    res = ioctl(appFd, FIONREAD, &bytesAvailable);      // how many bytes we can read immediately?

    if(res < 0) {                                       // ioctl() failed? no bytes available
        bytesAvailable = 0;
    }

    if(bytesAvailable > 0) {                            // something to read?
        int readCount = MIN(bytesAvailable, maxLen);    // lower value of these two is what we can read

        ssize_t res = sockRead(appFd, (char *) bfr, readCount);

        bfr += res;         // move pointer forward
        totalCnt += res;    // update the count
    }

    *bfr++ = 0;         // store update components - not updating anything
    totalCnt++;

    return totalCnt;                                    // return the count of bytes used
}

void ConsoleAppsStream::connectIfNeeded(void)
{
    if(appFd > 0) {             // do nothing if connected
        return;
    }

    appFd = openSocket();       // try to connect

    if(appFd != -1) {
        hasConnected = true;
    }
}

int ConsoleAppsStream::openSocket(void)
{
    char sockPath[128];

    // construct filename for socket, e.g. /var/run/ce/app0.sock
    std::string sockPathFormat = Utils::dotEnvValue("APPS_VIA_SOCK_PATHS");     // fetch format for app-via-sock path to sock
    sprintf(sockPath, sockPathFormat.c_str(), appIndex);                    // create path for nextAppIndex

    struct sockaddr_un addr;

    // Create a new client socket with domain: AF_UNIX, type: SOCK_STREAM, protocol: 0
    int sfd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (sfd == -1) {
        return -1;
    }

    // Construct server address, and make the connection.
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sockPath, sizeof(addr.sun_path) - 1);

    // TODO: set 500 ms timeout!

    if (connect(sfd, (struct sockaddr *) &addr, sizeof(struct sockaddr_un)) == -1) {
        return -1;
    }

    return sfd;
}

void ConsoleAppsStream::closeFd(int& fd)
{
    if(fd > 0) {
        close(fd);
        fd = 0;
    }
}

ssize_t ConsoleAppsStream::sockRead(int& fdIn, char* bfr, int bfrLen)
{
    if(fdIn < 0) {      // not connected? quit
        return 0;
    }

    int bytesAvailable = 0;
    int ires = ioctl(fdIn, FIONREAD, &bytesAvailable);       // how many bytes we can read?

    if(ires == -1 || bytesAvailable < 1) {
        return 0;
    }

    size_t bytesRead = MIN(bytesAvailable, bfrLen); // how many bytes can we read into buffer?
    ssize_t sres = 0;

    sres = recv(fdIn, bfr, bytesRead, MSG_NOSIGNAL);      // read to buffer

    if(sres < 0 && errno == EPIPE) {            // close fd when socket was closed
        closeFd(fdIn);
        return 0;
    }

    return sres;
}

ssize_t ConsoleAppsStream::sockWrite(int& fdOut, char* bfr, int count)
{
    if(fdOut < 0) {      // not connected? quit
        return 0;
    }

    ssize_t sres = send(fdOut, (const void *)bfr, count, MSG_NOSIGNAL);

    if(sres < 0 && errno == EPIPE) {            // close fd when socket was closed
        closeFd(fdOut);
        return 0;
    }

    return sres;
}

void ConsoleAppsStream::atariKeyToConsoleKey(uint8_t atariKey, char *bfr, int &cnt)
{
    // TODO : translate to ASCII ? (vt52 behaviour)
    // DELETE = 0x7F  BACKSPACE = Ctrl+H = 0x08
    // see https://en.wikipedia.org/wiki/VT52
    switch(atariKey) {
        case KEY_UP:        strcpy(bfr, "\x1b\x5b\x41");            break;
        case KEY_DOWN:      strcpy(bfr, "\x1b\x5b\x42");            break;
        case KEY_RIGHT:     strcpy(bfr, "\x1b\x5b\x43");            break;
        case KEY_LEFT:      strcpy(bfr, "\x1b\x5b\x44");            break;
        case KEY_ENTER:     strcpy(bfr, "\x0a");                    break;
        case KEY_ESC:       strcpy(bfr, "\x1b");                    break;
        case KEY_DELETE:    strcpy(bfr, "\x1b\x5b\x33\x7e");        break;
        case KEY_BACKSP:    strcpy(bfr, "\x7f");                    break;
        case KEY_TAB:       strcpy(bfr, "\x09");                    break;
        case KEY_INSERT:    strcpy(bfr, "\x1b\x5b\x32\x7e");        break;
        case KEY_HOME:      strcpy(bfr, "\x1b\x5b\x31\x7e");        break;
        case KEY_F1:        strcpy(bfr, "\x1b\x5b\x31\x31\x7e");    break;
        case KEY_F2:        strcpy(bfr, "\x1b\x5b\x31\x32\x7e");    break;
        case KEY_F3:        strcpy(bfr, "\x1b\x5b\x31\x33\x7e");    break;
        case KEY_F4:        strcpy(bfr, "\x1b\x5b\x31\x34\x7e");    break;
        case KEY_F5:        strcpy(bfr, "\x1b\x5b\x31\x35\x7e");    break;
        case KEY_F6:        strcpy(bfr, "\x1b\x5b\x31\x37\x7e");    break;
        case KEY_F7:        strcpy(bfr, "\x1b\x5b\x31\x38\x7e");    break;
        case KEY_F8:        strcpy(bfr, "\x1b\x5b\x31\x39\x7e");    break;
        case KEY_F9:        strcpy(bfr, "\x1b\x5b\x32\x30\x7e");    break;
        case KEY_F10:       strcpy(bfr, "\x1b\x5b\x32\x31\x7e");    break;

        case KEY_HELP:
        case KEY_UNDO:      bfr[0] = 0x00; break;

        default:    // if came here, not a special key, just copy it
            bfr[0] = atariKey;
            bfr[1] = 0;
            break;
    }

    cnt = strlen(bfr);      // get converted sequence count
}
