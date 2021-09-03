// vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <queue>
#include <pty.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <termios.h>

#include "global.h"
#include "debug.h"
#include "utils.h"
#include "config/config_commands.h"
#include "config/configstream.h"
#include "config/keys.h"

#include "ce_conf_on_rpi.h"
#include "periodicthread.h"

extern SharedObjects shared;

#define INBFR_SIZE  (10 * 1024)
uint8_t *inBfr;
uint8_t *tmpBfr;

extern bool otherInstanceIsRunning(void);

int translateVT52toVT100(uint8_t *bfr, uint8_t *tmp, int cnt)
{
    int i, t = 0;

    memset(tmp, 0, INBFR_SIZE);

    for(i=0; i<cnt; ) {
        if(bfr[i] == 27) {
            switch(bfr[i + 1]) {
                case 'E':               // clear screen
                    // set non-inverted colors
                    strcat((char *) tmp, "\033[37m");        // foreground white
                    strcat((char *) tmp, "\033[40m");        // background black
                    strcat((char *) tmp, "\033[2J");         // clear whole screen
                    strcat((char *) tmp, "\033[H");          // position cursor to 0,0

                    t += 5 + 5 + 4 + 3;
                    i += 2;
                    break;
                //------------------------
                case 'Y':               // goto position
                    int x, y;
                    y = bfr[i+2] - 32;
                    x = bfr[i+3] - 32;

                    char tmp2[16];
                    sprintf(tmp2, "\033[%d;%dH", y, x);
                    strcat((char *) tmp, tmp2);
                    t += strlen(tmp2);

                    i += 4;
                    break;
                //------------------------
                case 'p':                           // inverse on
                    strcat((char *) tmp, "\033[30m");      // foreground black
                    strcat((char *) tmp, "\033[47m");      // background white

                    t += 5 + 5;
                    i += 2;
                    break;
                //------------------------
                case 'q':                           // inverse off
                    strcat((char *) tmp, "\033[37m");      // foreground white
                    strcat((char *) tmp, "\033[40m");      // background black

                    t += 5 + 5;
                    i += 2;
                    break;
                //------------------------
                case 'e':               // cursor on
                    strcat((char *) tmp, "\033[?25h");

                    t += 6;
                    i += 2;
                    break;
                //------------------------
                case 'f':               // cursor off
                    strcat((char *) tmp, "\033[?25l");

                    t += 6;
                    i += 2;
                    break;
                //------------------------
                default:
                    printf("Unknown ESC sequence: %02d %02d \n", bfr[i], bfr[i+1]);
                    i += 2;
                    break;
            }
        } else {
            tmp[t++] = bfr[i++];
        }
    }

    memcpy(bfr, tmp, t);             // copy back the converted data
    return t;
}

static int termFd1;
static int termFd2;

static void emptyFd(int fd, uint8_t *bfr);
static bool receiveStream(int byteCount, uint8_t *data, int fd);

bool sendCmd(uint8_t cmd, uint8_t param, int fd1, int fd2, uint8_t *dataBuffer, uint8_t *tempBuffer, int &vt100byteCount)
{
    char bfr[3];
    int  res;

    vt100byteCount = 0;

    emptyFd(fd2, inBfr);                                        // first check if there isn't something stuck in the fifo, and if there is, just read it to discard it

    bfr[0] = HOSTMOD_CONFIG;
    bfr[1] = cmd;
    bfr[2] = param;

    res = write(fd1, bfr, 3);                                   // send command

    if(res != 3) {
        printf("sendCmd -- write failed!\n");
        return false;
    }

    uint16_t howManyBytes;

    bool bRes = receiveStream(2, (uint8_t *) &howManyBytes, fd2);  // first receive byte count that we should read

    if(!bRes) {                                                 // didn't receive available byte count?
        bRes = otherInstanceIsRunning();                        // is the main app running?

        if(!bRes) {                                             // the main app is not running? should quit now!
            write(STDOUT_FILENO, "\033[2J\033[H" , 7);          // clear whole screen, cursor up

            printf("The CosmosEx main app not running, terminating config tool!\n");
            sleep(3);
            sigintReceived = 1;
            return false;
        }

        printf("sendCmd -- failed to receive byte count\n");

        Debug::out(LOG_DEBUG, "sendCmd fail on receiving howManyBytes");
        return false;
    }

    if(howManyBytes < 1) {
        Debug::out(LOG_DEBUG, "sendCmd success because didn't need any other data");
        return true;
    }

    bRes = receiveStream(howManyBytes, dataBuffer, fd2);        // then receive the stream

    if(bRes) {
        vt100byteCount = translateVT52toVT100(dataBuffer, tempBuffer, howManyBytes);    // translate VT52 stream to VT100 stream

        Debug::out(LOG_DEBUG, "sendCmd success, %d bytes of VT52 translated to %d bytes of VT100", howManyBytes, vt100byteCount);
    } else {
        Debug::out(LOG_DEBUG, "sendCmd fail on getting VT52 stream");
    }

    return bRes;
}

static void emptyFd(int fd, uint8_t *bfr)
{
    while(1) {
        int bytesAvailable;
        int res = ioctl(fd, FIONREAD, &bytesAvailable); // how many bytes we can read?

        if(res != -1 && bytesAvailable > 0) {           // ioctl success and something to read? read it, ignore it
            read(fd, bfr, bytesAvailable);
            Debug::out(LOG_DEBUG, "emptyFd - read %d bytes", bytesAvailable);
        } else {                                        // nothing more to read, quit this loop and continue with the rest
            break;
        }
    }
}

static bool receiveStream(int byteCount, uint8_t *data, int fd)
{
    ssize_t n;
    fd_set readfds;
    struct timeval timeout;
    uint32_t timeOutTime = Utils::getEndTime(1000);

    int recvCount = 0;          // how many VT52  chars we already got

    while(recvCount < byteCount) {                                          // receive all the data, wait up to 1 second to receive it
        uint32_t now = Utils::getCurrentMs();
        if(now >= timeOutTime) {                          // time out happened, nothing received within specified timeout? fail
            Debug::out(LOG_DEBUG, "receiveStream - fail, wanted %d and got only %d bytes", byteCount, recvCount);
            return false;
        }
        memset(&timeout, 0, sizeof(timeout));
        timeout.tv_sec = (timeOutTime - now) / 1000;
        timeout.tv_usec = ((timeOutTime - now) % 1000) * 1000;
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);
        if(select(fd + 1, &readfds, NULL, NULL, &timeout) < 0) {
            Debug::out(LOG_ERROR, "receiveStream - select(): %s", strerror(errno));
            continue;
        }
        if(FD_ISSET(fd, &readfds)) {
            n = read(fd, data, (unsigned)(byteCount - recvCount));
            Debug::out(LOG_DEBUG, "receiveStream - read: %d", (int)n);
            if(n < 0) {
                if(errno == EAGAIN)
                    continue;
                Debug::out(LOG_ERROR, "receiveStream - read(): %s", strerror(errno));
                return false;
            } else if(n == 0) {
                Debug::out(LOG_ERROR, "receiveStream - read(): no more to read");
                return 0;
            } else {
                data += n;
                recvCount += n;
                data[0] = '\0';
            }
        }
    }

    Debug::out(LOG_DEBUG, "receiveStream - success, %d bytes", recvCount);
    return true;
}

static int safe_getchar(int *count) {
    if (*count > 0) {
        --(*count);
        return getchar();
    } else {
        return 0;
    }
}

static uint8_t getKey(int count)
{
    int c = safe_getchar(&count);

    if(c != 27) {           // not ESC sequence? just return value
        switch(c) {
            case 0x0a:  return KEY_ENTER;
            case 0x7f:  return KEY_BACKSP;
            case 0x09:  return KEY_TAB;
        }

        return c;
    }

    // if we came here, it's ESC or ESC sequence
    if(count == 0) {        // just esc? return it
        return KEY_ESC;
    }
    int a = safe_getchar(&count);
    int b = safe_getchar(&count);

    int res = 0;
    if(a == 0x5b) {
        switch(b) {
            case 0x41: res = KEY_UP;    break;
            case 0x42: res = KEY_DOWN;  break;
            case 0x43: res = KEY_RIGHT; break;
            case 0x44: res = KEY_LEFT;  break;

            case 0x31:
                c = safe_getchar(&count);
                if(c == 0x7e) {
                    res = KEY_HOME;
                } else if(c == 0x35) {
                    c = safe_getchar(&count);
                    if(c == 0x7e) {
                        res = KEY_F5;
                    }
                } else if(c == 0x37) {
                    c = safe_getchar(&count);
                    if(c == 0x7e) {
                        res = KEY_F6;
                    }
                } else if(c == 0x38) {
                    c = safe_getchar(&count);
                    if(c == 0x7e) {
                        res = KEY_F7;
                    }
                } else if(c == 0x39) {
                    c = safe_getchar(&count);
                    if(c == 0x7e) {
                        res =KEY_F8;
                    }
                }
                break;

            case 0x32:
                c = safe_getchar(&count);
                if(c == 0x7e) {
                    res =KEY_INSERT;
                } else if(c == 0x30) {
                    c = safe_getchar(&count);
                    if(c == 0x7e) {
                        res = KEY_F9;
                    }
                } else if(c == 0x31) {
                    c = safe_getchar(&count);
                    if(c == 0x7e) {
                        res = KEY_F10;
                    }
                }
                break;

            case 0x33:
                c = safe_getchar(&count);
                if(c == 0x7e) {
                    res = KEY_DELETE;
                }
                break;

            case 0x34:
                c = safe_getchar(&count);
                if(c == 0x7e) {
                    res = KEY_END;
                }
                break;

            case 0x5a: res = KEY_SHIFT_TAB; break;
        }
    } else if (a == 0x4f) {
        switch (b) {
            case 0x50: res = KEY_F1; break;
            case 0x51: res = KEY_F2; break;
            case 0x52: res = KEY_F3; break;
            case 0x53: res = KEY_F4; break;
        }
    }

    return count == 0 ? res : 0;
}

void ce_conf_mainLoop(void)
{
    bool configNotLinuxConsole = true;

    inBfr   = new uint8_t[INBFR_SIZE];
    tmpBfr  = new uint8_t[INBFR_SIZE];

    termFd1 = open(FIFO_TERM_PATH1, O_RDWR);             // will be used for writing only
    termFd2 = open(FIFO_TERM_PATH2, O_RDWR);             // will be used for reading only

    if(termFd1 == -1 || termFd2 == -1) {
        printf("ce_conf_mainLoop -- open() failed: %s\n", strerror(errno));
        return;
    }

    bool res;
    int vt100count;
    res = sendCmd(CFG_CMD_SET_RESOLUTION, ST_RESOLUTION_HIGH, termFd1, termFd2, inBfr, tmpBfr, vt100count);

    if(res) {
        write(STDOUT_FILENO, (char *) inBfr, vt100count);
    }

    struct termios old_tio_in, new_tio_in;
    struct termios old_tio_out, new_tio_out;

    tcgetattr(STDIN_FILENO, &old_tio_in);           // get the terminal settings for stdin
    new_tio_in = old_tio_in;                        // we want to keep the old setting to restore them a the end
    new_tio_in.c_lflag &= (~ICANON & ~ECHO);        // disable canonical mode (buffered i/o) and local echo
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio_in);  // set the new settings immediately

    tcgetattr(STDOUT_FILENO,&old_tio_out);          // get the terminal settings for stdout
    new_tio_out = old_tio_out;                      // we want to keep the old setting to restore them a the end
    new_tio_out.c_lflag &= (~ICANON);               // disable canonical mode (buffered i/o)
    tcsetattr(STDOUT_FILENO,TCSANOW, &new_tio_out); // set the new settings immediately

    uint32_t lastUpdate = 0;
    uint32_t now;

    while(sigintReceived == 0) {
        fd_set readfds;
        struct timeval timeout;
        long time_ms;
        int max_fd;

        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        //FD_SET(fd, &readfds);
        max_fd = STDIN_FILENO;  //max_fd = MAX(STDIN_FILENO, fd);
        now = Utils::getCurrentMs();
        time_ms = lastUpdate + 1000 - now;
        if(time_ms < 0) time_ms = 0;
        memset(&timeout, 0, sizeof(timeout));
        timeout.tv_sec = time_ms / 1000;
        timeout.tv_usec = (time_ms % 1000) * 1000;
        if(select(max_fd + 1, &readfds, NULL, NULL, &timeout) < 0) {
            if(errno != EINTR) {
                Debug::out(LOG_ERROR, "ce_conf_mainLoop - select() %s", strerror(errno));
            }
            continue;
        }
        if(FD_ISSET(STDIN_FILENO, &readfds)) {
            // check if something waiting from keyboard, and if so, read it
            int bytesAvailable;
            int res = ioctl(STDIN_FILENO, FIONREAD, &bytesAvailable);   // how many bytes we can read from keyboard?

            if(res != -1 && bytesAvailable > 0) {
                uint8_t key = getKey(bytesAvailable);                      // get the key in format valid for config components

                if(key == KEY_F10) {                                    // should quit? do it
                    break;
                }

                if(key == KEY_F8) {                                     // if should switch between config view and linux console view
                    write(STDOUT_FILENO, "\033[37m", 5);                // foreground white
                    write(STDOUT_FILENO, "\033[40m", 5);                // background black
                    write(STDOUT_FILENO, "\033[2J" , 4);                // clear whole screen
                    write(STDOUT_FILENO, "\033[H"  , 3);                // position cursor to 0,0

                    configNotLinuxConsole = !configNotLinuxConsole;
                    key = 0;
                }

                if(key != 0) {                                          // if got the key, send key down event
                    res = sendCmd(configNotLinuxConsole ? CFG_CMD_KEYDOWN : CFG_CMD_LINUXCONSOLE_GETSTREAM, key, termFd1, termFd2, inBfr, tmpBfr, vt100count);

                    if(res) {
                        int len = strnlen((const char *) inBfr, vt100count);    // get real string length - might have 2 more bytes (isUpdateScreen, updateComponents) after string terminator
                        write(STDOUT_FILENO, (char *) inBfr, len);
                    }

                    lastUpdate = Utils::getCurrentMs();                 // store current time as we just updated
                }
            }
        }

        //-----------
        // should do refresh or nothing?
        if(Utils::getCurrentMs() - lastUpdate >= 1000) {            // last update more than 1 second ago? refresh
            res = sendCmd(configNotLinuxConsole ? CFG_CMD_REFRESH : CFG_CMD_LINUXCONSOLE_GETSTREAM, 0, termFd1, termFd2, inBfr, tmpBfr, vt100count);

            if(res) {
                int len = strnlen((const char *) inBfr, vt100count);    // get real string length - might have 2 more bytes (isUpdateScreen, updateComponents) after string terminator
                write(STDOUT_FILENO, (char *) inBfr, len);
            }

            lastUpdate = Utils::getCurrentMs();                     // store current time as we just updated
        }
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio_in);      // restore the former settings
    tcsetattr(STDOUT_FILENO, TCSANOW, &old_tio_out);    // restore the former settings

    delete []inBfr;
    delete []tmpBfr;

    system("reset");
}
void ce_conf_createFifos(ConfigPipes *cp, char *path1, char *path2);

void ce_conf_createFifos(void)
{
    ce_conf_createFifos(&shared.configPipes.web,    (char *) FIFO_WEB_PATH1,     (char *) FIFO_WEB_PATH2);
    ce_conf_createFifos(&shared.configPipes.term,   (char *) FIFO_TERM_PATH1,    (char *) FIFO_TERM_PATH2);
}

void ce_conf_createFifos(ConfigPipes *cp, char *path1, char *path2)
{
    int res, res2;

    res = mkfifo(path1, 0666);

    if(res != 0 && errno != EEXIST) {               // if mkfifo failed, and it's not 'file exists' error
        Debug::out(LOG_ERROR, "ce_conf_createFifos -- mkfifo() failed, errno: %d", errno);
        return;
    }

    res = mkfifo(path2, 0666);

    if(res != 0 && errno != EEXIST) {               // if mkfifo failed, and it's not 'file exists' error
        Debug::out(LOG_ERROR, "ce_conf_createFifos -- mkfifo() failed, errno: %d", errno);
        return;
    }

    cp->fd1 = open(path1, O_RDWR);              // will be used for reading only
    cp->fd2 = open(path2, O_RDWR);              // will be used for writing only

    if(cp->fd1 == -1 || cp->fd2 == -1) {
        Debug::out(LOG_ERROR, "ce_conf_createFifos -- open() failed");
        return;
    }

    res     = fcntl(cp->fd1, F_SETFL, O_NONBLOCK);
    res2    = fcntl(cp->fd2, F_SETFL, O_NONBLOCK);

    if(res == -1 || res2 == -1) {
        Debug::out(LOG_ERROR, "ce_conf_createFifos -- fcntl() failed");
        return;
    }

    Debug::out(LOG_DEBUG, "ce_conf FIFOs created");
}
