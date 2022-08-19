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
#include "configstream.h"
#include "config_commands.h"
#include "../debug.h"

#define APP_SOCK_PATH   "/var/run/ce"

uint8_t isUpdateStartingFlag = 0;

ConfigStream::ConfigStream()
{
    stScreenWidth   = 40;
    gotoOffset      = 0;

    dataTrans   = NULL;
    reloadProxy = NULL;

    lastCmdTime = 0;
    appIndex = 0;       // start with app0
    appFd = -1;         // not connected
}

ConfigStream::~ConfigStream()
{

}

void ConfigStream::setAcsiDataTrans(AcsiDataTrans *dt)
{
    dataTrans = dt;
}

void ConfigStream::setSettingsReloadProxy(SettingsReloadProxy *rp)
{
    reloadProxy = rp;
}

void ConfigStream::processCommand(uint8_t *cmd)
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
        if(cmd[5] != 0) {                   // key was really pressed, send it
            char consoleKeys[10];
            int  consoleKeysLength = 0;
            atariKeyToConsoleKey(cmd[5], consoleKeys, consoleKeysLength);   // convert key from Atari pseudo key to console key
            sockWrite(appFd, consoleKeys, consoleKeysLength);               // send to app
        }

        streamCount = getStream(readBuffer, READ_BUFFER_SIZE);              // then get current screen stream
        streamCount = filterVT100((char *) readBuffer, streamCount);        // VT100 to VT52

        dataTrans->addDataBfr(readBuffer, streamCount, true);               // add data and status, with padding to multiple of 16 bytes
        dataTrans->setStatus(SCSI_ST_OK);

        Debug::out(LOG_DEBUG, "handleConfigStream -- CFG_CMD_KEYDOWN -- %d bytes", streamCount);
        break;

    case CFG_CMD_SET_RESOLUTION:
        switch(cmd[5]) {
        case ST_RESOLUTION_LOW:     stScreenWidth = 40; break;
        case ST_RESOLUTION_MID:
        case ST_RESOLUTION_HIGH:    stScreenWidth = 80; break;
        }

        gotoOffset = (stScreenWidth - 40) / 2;

        dataTrans->setStatus(SCSI_ST_OK);
        Debug::out(LOG_DEBUG, "handleConfigStream -- CFG_CMD_SET_RESOLUTION -- %d", cmd[5]);
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

        Debug::out(LOG_DEBUG, "handleConfigStream -- CFG_CMD_UPDATING_QUERY -- isUpdateStartingFlag: %d, updateComponentsWithValidityNibble: %x", isUpdateStartingFlag, updateComponentsWithValidityNibble);
        break;
    }

    case CFG_CMD_GET_APP_NAMES:
        // TODO: read app names from file and send them to ST
        break;

    case CFG_CMD_SET_APP_INDEX:
        // TODO: set current app index, disconnect, connect to new app sock
        break;

    default:                            // other cases: error
        dataTrans->setStatus(SCSI_ST_CHECK_CONDITION);
        break;
    }

    dataTrans->sendDataAndStatus();     // send all the stuff after handling, if we got any
}

int ConfigStream::getStream(uint8_t *bfr, int maxLen)
{
    memset(bfr, 0, maxLen);                             // clear the buffer

    int totalCnt = 0;

    // first turn off the cursor to avoid cursor blinking on the screen
    *bfr++ = 27;
    *bfr++ = 'f';       // CUR_OFF
    totalCnt += 2;

    ssize_t res = sockRead(appFd, (char *) bfr, maxLen);

    bfr += res;     // move pointer forward 

    *bfr++ = 0;     // store update components - not updating anything
    totalCnt++;

    return totalCnt;                                    // return the count of bytes used
}

void ConfigStream::connectIfNeeded(void)
{
    if(appFd > 0) {             // do nothing if connected
        return;
    }

    appFd = openSocket();       // try to connect
}

int ConfigStream::openSocket(void)
{
    char fullname[128];

    // construct filename for socket, e.g. /var/run/ce/app0.sock
    sprintf(fullname, "%s/app%d.sock", APP_SOCK_PATH, appIndex);

    struct sockaddr_un addr;

    // Create a new client socket with domain: AF_UNIX, type: SOCK_STREAM, protocol: 0
    int sfd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (sfd == -1) {
        return -1;
    }

    // Construct server address, and make the connection.
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, fullname, sizeof(addr.sun_path) - 1);

    // TODO: set 500 ms timeout!

    if (connect(sfd, (struct sockaddr *) &addr, sizeof(struct sockaddr_un)) == -1) {
        return -1;
    }

    return sfd;
}

void ConfigStream::closeFd(int& fd)
{
    if(fd > 0) {
        close(fd);
        fd = 0;
    }
}

ssize_t ConfigStream::sockRead(int& fdIn, char* bfr, int bfrLen)
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

ssize_t ConfigStream::sockWrite(int& fdOut, char* bfr, int count)
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

int ConfigStream::filterVT100(char *bfr, int cnt)
{
    static char tmp[READ_BUFFER_SIZE];
    int i = 0, j = 0;

    while(i < cnt) {
        if(bfr[i] == 27 && bfr[i + 1] == '[') {                         // if it's VT100 cmd
            int move = 2;                                               // in case we wouldn't recognize this sequence, move beyond Esc [

            if(bfr[i + 3] == 'm') {                                     // general text attributes? skip them
                move = 4;
            } else if(bfr[i + 4] == 'm') {                              // foreground / background color? skip it
                move = 5;
            } else if(bfr[i + 5] == 'm') {                              // background color? skip it
                move = 6;
            } else if(bfr[i + 2] == 'K') {                              // clear line from current cursor pos to end
                tmp[j++] = 27;
                tmp[j++] = 75;
                move = 3;
            } else if(bfr[i + 3] == 'K') {                              // clear line
                tmp[j++] = 27;

                switch(bfr[i + 2]) {
                    case '0':   tmp[j++] = 75;  break;                  // clear from cursor to right
                    case '1':   tmp[j++] = 111; break;                  // clear from cursor to left
                    case '2':   tmp[j++] = 108; break;                  // clear whole line
                }

                move = 4;
            } else if(strcmp(bfr + 2, "38;5;") == 0 || strcmp(bfr + 2, "48;5;") == 0) {     // it's 88 or 256 colors command?
                for(int k=7; k<14; k++) {                                                   // find the terminating 'm' character
                    if(bfr[i + k] == 'm') {                                                 // found? move one beyond that
                        move = k + 1;
                        break;
                    }
                }
            } else {                                                                        // in other cases - it might be a color terminated by 'm'
                for(int k=2; k<10; k++) {                                                   // find the terminating 'm' character
                    if(bfr[i + k] == 'm') {                                                 // found? move one beyond that
                        move = k + 1;
                        break;
                    }
                }
            }

            i += move;                                                  // move beyond this VT100 command
        } else {                                                        // not VT100 cmd, copy it
            tmp[j] = bfr[i];
            i++;
            j++;
        }
    }

    memcpy(bfr, tmp, j);                                                // now copy the filtered data
    return j;
}

void ConfigStream::atariKeyToConsoleKey(uint8_t atariKey, char *bfr, int &cnt)
{
    // TODO : translate to ASCII ? (vt52 behaviour)
    // DELETE = 0x7F  BACKSPACE = Ctrl+H = 0x08
    // see https://en.wikipedia.org/wiki/VT52
    switch(atariKey) {
        case KEY_UP:        bfr[0] = 0x1b; bfr[1] = 0x5b; bfr[2] = 0x41; cnt = 3; return;
        case KEY_DOWN:      bfr[0] = 0x1b; bfr[1] = 0x5b; bfr[2] = 0x42; cnt = 3; return;
        case KEY_LEFT:      bfr[0] = 0x1b; bfr[1] = 0x5b; bfr[2] = 0x44; cnt = 3; return;
        case KEY_RIGHT:     bfr[0] = 0x1b; bfr[1] = 0x5b; bfr[2] = 0x43; cnt = 3; return;
        case KEY_ENTER:     bfr[0] = 0x0a; cnt = 1; return;
        case KEY_ESC:       bfr[0] = 0x1b; cnt = 1; return;
        case KEY_DELETE:    bfr[0] = 0x1b; bfr[1] = 0x5b; bfr[2] = 0x33; bfr[3] = 0x7e; cnt = 4; return;
        case KEY_BACKSP:    bfr[0] = 0x7f; cnt = 1; return;
        case KEY_TAB:       bfr[0] = 0x09; cnt = 1; return;
        case KEY_INSERT:    bfr[0] = 0x1b; bfr[1] = 0x5b; bfr[2] = 0x32; bfr[3] = 0x7e; cnt = 4; return;
        case KEY_HOME:      bfr[0] = 0x1b; bfr[1] = 0x5b; bfr[2] = 0x31; bfr[3] = 0x7e; cnt = 4; return;
        case KEY_HELP:      bfr[0] = 0x00; cnt = 0; return;
        case KEY_UNDO:      bfr[0] = 0x00; cnt = 0; return;
        case KEY_F1:        bfr[0] = 0x1b; bfr[1] = 0x5b; bfr[2] = 0x31; bfr[3] = 0x31; bfr[4] = 0x7e; cnt = 5; return;
        case KEY_F2:        bfr[0] = 0x1b; bfr[1] = 0x5b; bfr[2] = 0x31; bfr[3] = 0x32; bfr[4] = 0x7e; cnt = 5; return;
        case KEY_F3:        bfr[0] = 0x1b; bfr[1] = 0x5b; bfr[2] = 0x31; bfr[3] = 0x33; bfr[4] = 0x7e; cnt = 5; return;
        case KEY_F4:        bfr[0] = 0x1b; bfr[1] = 0x5b; bfr[2] = 0x31; bfr[3] = 0x34; bfr[4] = 0x7e; cnt = 5; return;
        case KEY_F5:        bfr[0] = 0x1b; bfr[1] = 0x5b; bfr[2] = 0x31; bfr[3] = 0x35; bfr[4] = 0x7e; cnt = 5; return;
        case KEY_F6:        bfr[0] = 0x1b; bfr[1] = 0x5b; bfr[2] = 0x31; bfr[3] = 0x37; bfr[4] = 0x7e; cnt = 5; return;
        case KEY_F7:        bfr[0] = 0x1b; bfr[1] = 0x5b; bfr[2] = 0x31; bfr[3] = 0x38; bfr[4] = 0x7e; cnt = 5; return;
        case KEY_F8:        bfr[0] = 0x1b; bfr[1] = 0x5b; bfr[2] = 0x31; bfr[3] = 0x39; bfr[4] = 0x7e; cnt = 5; return;
        case KEY_F9:        bfr[0] = 0x1b; bfr[1] = 0x5b; bfr[2] = 0x32; bfr[3] = 0x30; bfr[4] = 0x7e; cnt = 5; return;
        case KEY_F10:       bfr[0] = 0x1b; bfr[1] = 0x5b; bfr[2] = 0x32; bfr[3] = 0x31; bfr[4] = 0x7e; cnt = 5; return;
        default:
            // if came here, not a special key, just copy it
            bfr[0] = atariKey;
            cnt = 1;
    }
}
