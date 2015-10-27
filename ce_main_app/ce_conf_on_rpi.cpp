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

int ce_conf_fd1;
int ce_conf_fd2;

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

static bool sendCmd(BYTE cmd, BYTE param)
{
    char bfr[3];
    int  res;
    static int noReplies = 0;
    
    bfr[0] = HOSTMOD_CONFIG;
    bfr[1] = cmd;
    bfr[2] = param;
    
    res = write(ce_conf_fd1, bfr, 3);
    
    if(res != 3) {
        printf("sendCmd -- write failed!\n");
        return false;
    }
    
    Utils::sleepMs(50);
    
    int bytesAvailable;
    res = ioctl(ce_conf_fd2, FIONREAD, &bytesAvailable);         // how many bytes we can read?

    if(res != -1 && bytesAvailable > 0) {
        int readCount = (bytesAvailable < INBFR_SIZE) ? bytesAvailable : INBFR_SIZE;
        
        memset(inBfr, 0, INBFR_SIZE);
        res = read(ce_conf_fd2, inBfr, readCount);

        Debug::out(LOG_DEBUG, "sendCmd - readCount: %d", readCount);
        
        int newCount = translateVT52toVT100(inBfr, tmpBfr, readCount);
        
        write(STDOUT_FILENO, inBfr, newCount);
        return true;
    } else {
        printf("\033[2J\033[HCosmosEx app does not reply, is it running?\n");
        noReplies++;
        
        if(noReplies >= 5) {
            printf("No response from CosmosEx app in 5 attempts, terminating.\nThe CosmosEx app must be running for CE_CONF to work.\n");
            return false;
        }
        
        return true;
    }
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
    inBfr   = new BYTE[INBFR_SIZE];
    tmpBfr  = new BYTE[INBFR_SIZE];
    
    ce_conf_fd1 = open(FIFO_PATH1, O_RDWR);             // will be used for writing only
    ce_conf_fd2 = open(FIFO_PATH2, O_RDWR);             // will be used for reading only

    if(ce_conf_fd1 == -1 || ce_conf_fd2 == -1) {
        printf("ce_conf_mainLoop -- open() failed\n");
        return;
    }
    
    sendCmd(CFG_CMD_SET_RESOLUTION, ST_RESOLUTION_HIGH);
    
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
        int bytesAvailable;
        int res = ioctl(STDIN_FILENO, FIONREAD, &bytesAvailable);           // how many bytes we can read?
    
        if(res != -1 && bytesAvailable > 0) {
            BYTE key = getKey(bytesAvailable);                              // get the key in format valid for config components
            
            if(key == KEY_F10) {                                            // should quit? do it
                break;
            }
            
            if(key != 0) {                                                  // if got the key, send key down event
                sendCmd(CFG_CMD_KEYDOWN, key);
                lastUpdate = Utils::getCurrentMs();                         // store current time as we just updated
            }
        }
        
        if(Utils::getCurrentMs() - lastUpdate >= 1000) {                    // last update more than 1 second ago? refresh
            bool res = sendCmd(CFG_CMD_REFRESH, 0);
            lastUpdate = Utils::getCurrentMs();                             // store current time as we just updated
        
            if(!res) {  
                break;
            }
        } else {                                                            // if we updated less than 1 second ago, sleep 50 ms
            Utils::sleepMs(50);
        }
    }
    
	tcsetattr(STDIN_FILENO, TCSANOW, &old_tio_in);      // restore the former settings
	tcsetattr(STDOUT_FILENO, TCSANOW, &old_tio_out);    // restore the former settings
    
    delete []inBfr;
    delete []tmpBfr;
    
    system("reset");
}

void ce_conf_createFifos(void)
{
    int res, res2;

    res = mkfifo(FIFO_PATH1, 0666);

    if(res != 0 && errno != EEXIST) {                   // if mkfifo failed, and it's not 'file exists' error
        Debug::out(LOG_ERROR, "ce_conf_createFifos -- mkfifo() failed, errno: %d", errno);
        return;
    }

    res = mkfifo(FIFO_PATH2, 0666);

    if(res != 0 && errno != EEXIST) {                   // if mkfifo failed, and it's not 'file exists' error
        Debug::out(LOG_ERROR, "ce_conf_createFifos -- mkfifo() failed, errno: %d", errno);
        return;
    }

    ce_conf_fd1 = open(FIFO_PATH1, O_RDWR);             // will be used for reading only
    ce_conf_fd2 = open(FIFO_PATH2, O_RDWR);             // will be used for writing only

    if(ce_conf_fd1 == -1 || ce_conf_fd2 == -1) {
        Debug::out(LOG_ERROR, "ce_conf_createFifos -- open() failed");
        return;
    }

    res     = fcntl(ce_conf_fd1, F_SETFL, O_NONBLOCK);
    res2    = fcntl(ce_conf_fd2, F_SETFL, O_NONBLOCK);

    if(res == -1 || res2 == -1) {
        Debug::out(LOG_ERROR, "ce_conf_createFifos -- fcntl() failed");
        return;
    }
    
    Debug::out(LOG_DEBUG, "ce_conf FIFOs created");
    
    // now create the ce_conf starting script if it doesn't exist
    res = access("/ce/ce_conf.sh", F_OK);

    if(res == -1) {         // file doesn't exist? create it
        system("echo -e '#!/bin/sh \n/ce/app/cosmosex ce_conf \nreset\n ' > /ce/ce_conf.sh");
        system("chmod 755 /ce/ce_conf.sh");
        
        Utils::forceSync();
    }
}
