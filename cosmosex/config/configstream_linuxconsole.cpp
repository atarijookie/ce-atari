#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <utmp.h>
#include <pty.h>

#include "../global.h"
#include "../native/scsi_defs.h"
#include "../acsidatatrans.h"

#include "../settings.h"
#include "keys.h"
#include "configstream.h"
#include "../debug.h"

extern int linuxConsole_fdMaster, linuxConsole_fdSlave;                 // file descriptors for pty pair

void ConfigStream::linuxConsole_KeyDown(BYTE atariKey)
{
    if(linuxConsole_fdMaster <= 0) {                                    // if don't have the handle, quit
        return;
    }

    char consoleKeys[10];
    int  consoleKeysLength = 0;
    
    atariKeyToConsoleKey(atariKey, consoleKeys, consoleKeysLength);     // convert it from Atari pseudo key to console key
    
    write(linuxConsole_fdMaster, consoleKeys, consoleKeysLength);       // send the key
}

int ConfigStream::linuxConsole_getStream(BYTE *bfr, int maxLen)
{
    int totalCnt = 0;
    memset(bfr, 0, maxLen);								                // clear the buffer
    
    bool maxData = false;                                               // flag that we will send full buffer

    if(linuxConsole_fdMaster <= 0) {                                    // can't read data from console? quit
        return 0;
    }
    
    int bytesAvailable;
    int ires = ioctl(linuxConsole_fdMaster, FIONREAD, &bytesAvailable); // how many bytes we can read?

    if(ires != -1 && bytesAvailable > 0) {
        int readCount;
        
        if(bytesAvailable <= (maxLen - 3)) {                            // can read less than buffer size?
            maxData         = false;
            readCount       = bytesAvailable;
            bfr[maxLen - 1] = LINUXCONSOLE_NO_MORE_DATA;                // last byte of buffer - no more data
        } else {                                                        // have more than 1 buffer to read?
            maxData         = true;
            readCount       = (maxLen - 3);
            bfr[maxLen - 1] = LINUXCONSOLE_GET_MORE_DATA;               // last byte of buffer - no more data
        }

        int rcnt = read(linuxConsole_fdMaster, bfr, readCount);         // read the data

        if(rcnt != -1) {                                                // if did reat the data
            int fcnt = filterVT100((char *) bfr, rcnt);                 // filter out those VT100 commands

            totalCnt     = fcnt;                                        // update total count of bytes
            bfr         += fcnt;                                        // advance in the buffer
        }
    }
	
    totalCnt += 2;                                                      // two zeros at the end -- string terminator and THIS_IS_NOT_UPDATE_SCREEN flag
    
    if(maxData) {                                                       // if sending full buffer, then set the count to max
        totalCnt = maxLen;
    }
    
    return totalCnt;                                                    // return the count of bytes used
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
                    if(bfr[k] == 'm') {                                                     // found? move one beyond that
                        move = k + 1;
                        break;
                    }
                }
            } else {                                                                        // in other cases - it might be a color terminated by 'm'
                for(int k=2; k<10; k++) {                                                   // find the terminating 'm' character
                    if(bfr[k] == 'm') {                                                     // found? move one beyond that
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

void ConfigStream::atariKeyToConsoleKey(BYTE atariKey, char *bfr, int &cnt)
{
    // first handle special keys
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
    }
    
    // if came here, not a special key, just copy it
    bfr[0] = atariKey;
    cnt = 1;
}


