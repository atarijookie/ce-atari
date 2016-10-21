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
#include <termios.h>

#include "global.h"
#include "debug.h"
#include "utils.h"
#include "config/configstream.h"
#include "config/keys.h"

#include "ce_conf_on_rpi.h" 
#include "periodicthread.h"

extern SharedObjects shared;

#define INBFR_SIZE  (10 * 1024)
BYTE *inBfr;
BYTE *tmpBfr;

int translateVT52toVT100(BYTE *bfr, BYTE *tmp, int cnt)
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

static void emptyFd(int fd, BYTE *bfr);
static bool receiveStream(int byteCount, BYTE *data, int fd);

bool sendCmd(BYTE cmd, BYTE param, int fd1, int fd2, BYTE *dataBuffer, BYTE *tempBuffer, int &vt100byteCount)
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

    WORD howManyBytes;
    
    bool bRes = receiveStream(2, (BYTE *) &howManyBytes, fd2);  // first receive byte count that we should read

    if(!bRes) {
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

static void emptyFd(int fd, BYTE *bfr)
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

static bool receiveStream(int byteCount, BYTE *data, int fd)
{
    DWORD timeOutTime = Utils::getEndTime(1000);

    int recvCount = 0;          // how many VT52  chars we already got
    
    BYTE *ptr = data;
    
    while(recvCount < byteCount) {                                          // receive all the data, wait up to 1 second to receive it
        if(Utils::getCurrentMs() >= timeOutTime) {                          // time out happened, nothing received within specified timeout? fail
            Debug::out(LOG_DEBUG, "receiveStream - fail, wanted %d and got only %d bytes", byteCount, recvCount);
            return false;
        }
    
        int bytesAvailable;
        int res = ioctl(fd, FIONREAD, &bytesAvailable);                     // how many bytes we can read?

        if(res != -1 && bytesAvailable > 0) {                               // ioctl success and something to read?
            int restOfBytes = byteCount - recvCount;                        // calculate how many we still have to receive to match specified byteCount
            int readCount   = (bytesAvailable < restOfBytes) ? bytesAvailable : restOfBytes;        // read everything if it's less of what we need; read just restOfBytes if we need less than what's available

            memset(ptr, 0, readCount + 1);
            read(fd, ptr, readCount);                                      // get the stream

            recvCount   += readCount;
            ptr             += readCount;
            
            Debug::out(LOG_DEBUG, "receiveStream - readCount: %d", readCount);
        }
        
        Utils::sleepMs(10);     // sleep a little
    }
    
    Debug::out(LOG_DEBUG, "receiveStream - success, %d bytes", byteCount);
    return true;    
}

static BYTE getKey(int count)
{
    int c = getchar();
    
    if(c != 27) {           // not ESC sequence? just return value
        switch(c) {
            case 0x0a:  return KEY_ENTER;
            case 0x7f:  return KEY_BACKSP;
            case 0x09:  return KEY_TAB;
        }
    
        return c;
    }

    // if we came here, it's ESC or ESC sequence
    if(count == 1) {        // just esc? return it
        return KEY_ESC;
    }

    int a, b;
    a = getchar();
    b = getchar();
    
    if(a == 0x5b) {
        switch(b) {
            case 0x41:    return KEY_UP;
            case 0x42:    return KEY_DOWN;
            case 0x43:    return KEY_RIGHT;
            case 0x44:    return KEY_LEFT;
            
            case 0x31:
                c = getchar();
                if(c == 0x7e) {
                    return KEY_HOME;
                } else if(c == 0x37) {
                    c = getchar();
                    if(c == 0x7e) {
                        return KEY_F6;
                    }
                } else if(c == 0x38) {
                    c = getchar();
                    if(c == 0x7e) {
                        return KEY_F7;
                    }
                } else if(c == 0x39) {
                    c = getchar();
                    if(c == 0x7e) {
                        return KEY_F8;
                    }
                }
                break;

            case 0x32:
                c = getchar();
                if(c == 0x7e) {
                    return KEY_INSERT;
                } else if(c == 0x30) {
                    c = getchar();
                    if(c == 0x7e) {
                        return KEY_F9;
                    }
                } else if(c == 0x31) {
                    c = getchar();
                    if(c == 0x7e) {
                        return KEY_F10;
                    }
                }
                break;
                
            case 0x33:
                c = getchar();
                if(c == 0x7e) {
                    return KEY_DELETE;
                }
                break;
                
            case 0x5b:
                c = getchar();
                switch(c) {
                    case 0x41:  return KEY_F1;
                    case 0x42:  return KEY_F2;
                    case 0x43:  return KEY_F3;
                    case 0x44:  return KEY_F4;
                    case 0x45:  return KEY_F5;
                }
                break;
        }
    }

    return 0;
}

void ce_conf_mainLoop(void)
{
    bool configNotLinuxConsole = true;

    inBfr   = new BYTE[INBFR_SIZE];
    tmpBfr  = new BYTE[INBFR_SIZE];
    
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
    
    DWORD lastUpdate = Utils::getCurrentMs();
    
    while(sigintReceived == 0) {
        bool didSomething = false;
    
        // check if something waiting from keyboard, and if so, read it
        int bytesAvailable;
        int res = ioctl(STDIN_FILENO, FIONREAD, &bytesAvailable);   // how many bytes we can read from keyboard?
    
        if(res != -1 && bytesAvailable > 0) {
            didSomething = true;
            
            BYTE key = getKey(bytesAvailable);                      // get the key in format valid for config components
            
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
                    write(STDOUT_FILENO, (char *) inBfr, vt100count);
                }
                
                lastUpdate = Utils::getCurrentMs();                 // store current time as we just updated
            }
        }
        
        //-----------
        // should do refresh or nothing?
        if(Utils::getCurrentMs() - lastUpdate >= 1000) {            // last update more than 1 second ago? refresh
            didSomething = true;
        
            res = sendCmd(configNotLinuxConsole ? CFG_CMD_REFRESH : CFG_CMD_LINUXCONSOLE_GETSTREAM, 0, termFd1, termFd2, inBfr, tmpBfr, vt100count);
        
            if(res) {
                write(STDOUT_FILENO, (char *) inBfr, vt100count);
            }

            lastUpdate = Utils::getCurrentMs();                     // store current time as we just updated
        } 
        
        if(!didSomething) {         // if nothing happened in this loop, wait a little
            Utils::sleepMs(10);
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
