#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>


#define READ_BUFFER_SIZE 1024
void VT100log(char* bfr, int len);

void dumpCmd(const char* label, char cmd, char* bfr)
{
    if(cmd == 0x0a) {   // this is EOL char, the command is probably incomplete in our test environment, we can ignore it
        return;
    }

    printf("%s: %c (0x%02X) - ", label, cmd, cmd);
    printf("%02X %02X %02X %02X %02X %02X - ", bfr[0], bfr[1], bfr[2], bfr[3], bfr[4], bfr[5]);

    for(int i=0; i<6; i++) {
        printf("%c", (bfr[i] > 32) ? bfr[i] : '.');
    }

    printf("\n");
}

int getCmdLength(char* bfr, char cmd)
{
    // Function will look for the first occurrence of cmd in the supplied buffer and return length of the command.

    for(int i=2; i<15; i++) {       // after ESC + [ look for cmd
        if(bfr[i] == cmd) {         // cmd found at this index?
            return (i + 1);         // return length
        }
    }

    return 2;
}

bool readNumbersAndCommand(char* bfr, int& v1, int& v2, int& v3, int& v4, char& cmd, int& cmdLen)
{
    // Function tries to find VT100 commands, which have number;number;number in them and fills supplied vars with it.

    int res = 0;
    cmd = 0;
    cmdLen = 2;
    v1 = v2 = v3 = v4 = 0;

    res = sscanf(bfr + 2, "%d;%d;%d;%d%c", &v1, &v2, &v3, &v4, &cmd);   // 4 numbers + command?
    if(res == 5) {      // 4 numbers + cmd - OK?
        cmdLen = getCmdLength(bfr, cmd);
        return true;
    }

    res = sscanf(bfr + 2, "%d;%d;%d%c", &v1, &v2, &v3, &cmd);   // 3 numbers + command?
    if(res == 4) {      // 3 numbers + cmd - OK?
        cmdLen = getCmdLength(bfr, cmd);
        return true;
    }

    res = sscanf(bfr + 2, "%d;%d%c", &v1, &v2, &cmd);           // 2 numbers + command?
    if(res == 3) {      // 2 numbers + cmd - OK?
        cmdLen = getCmdLength(bfr, cmd);
        return true;
    }

    res = sscanf(bfr + 2, "%d%c", &v1, &cmd);                   // 1 number + command?
    if(res == 2) {      // 1 number + cmd - OK?
        cmdLen = getCmdLength(bfr, cmd);
        return true;
    }

    res = sscanf(bfr + 2, "%c", &cmd);                          // no number, just command?
    if(res == 1) {      // 1 number + cmd - OK?
        cmdLen = getCmdLength(bfr, cmd);
        return true;
    }

    // failed to get command with numbers
    return false;
}

void vt100shorter(char* bfrIn, char* bfrOut, int& moveIn, int& moveOut)
{
    // Function for handling shorter VT100 commands, which are ESC + something (without [)

    moveIn = 2;                 // for unknown commands will skip 2 bytes in input buffer
    moveOut = 0;                // not adding anything to output buffer by default

    switch(bfrIn[1]) {
        case '(':               // G0 designator - select alternative character sets
        case ')':               // G1 designator - select alternative character sets
            moveIn = 3;         // these commands are ignored, but are 3 bytes long
            break;

        // ignored commands
        case '7':               // save cursor position, graphic rendition, and character set
            break;

        default:
            dumpCmd("unknown ESC+x", bfrIn[1], bfrIn);
            break;
    }
}

void vt100CursorMove(char cmd, int count, int cmdLen, char* bfrOut, int& moveOut)
{
    // This method emulates the VT100 command 'move N times' by adding N times 'move 1 char'

    if(cmdLen == 3) {               // if it's ESC + [ + A (or B, C, D), length is 3 and count of movements is 1
        count = 1;
    }

    moveOut = count * 2;            // total = 2 bytes per count of moves

    for(int i=0; i<count; i++) {    // repeat 'move 1 char' the required count times
        bfrOut[i*2 + 0] = '\033';
        bfrOut[i*2 + 1] = cmd;
    }
}

void vt100longer(char* bfrIn, char* bfrOut, int& moveIn, int& moveOut)
{
    // Function for handling longer VT100 commands, which are ESC + [ + something

    moveIn = 3;                 // for unknown commands will skip 2 bytes in input buffer
    moveOut = 0;                // not adding anything to output buffer by default

    int v1, v2, v3, v4;
    char cmd;
    bool found;

    // try to find a command with numbers
    found = readNumbersAndCommand(bfrIn, v1, v2, v3, v4, cmd, moveIn);

    if(!found) {
        dumpCmd("readNumbersAndCommand failed", bfrIn[2], bfrIn);
        return;
    }

    if(cmd == '?') {    // if command is '?' (whole sequence is 'ESC [ ?'), it looks like the rest seems similar to normal command
        found = readNumbersAndCommand(bfrIn + 1, v1, v2, v3, v4, cmd, moveIn);  // try to read again after the '?'
        moveIn++;       // we moved 1 more in bfrIn because we've started after the '?' in this case

        if(!found) {
            dumpCmd("readNumbersAndCommand failed after '?'", bfrIn[2], bfrIn);
            moveIn = 3;
            return;
        }
    }

    moveOut = 2;    // we will be adding 2 bytes mostly, be sure to change this to 0 if not adding anything below

    switch(cmd) {
        case 'h': {
            switch(v1) {
                case 7:     memcpy(bfrOut, "\033v", 2); break;     // 7h - wrap ON
                case 25:    memcpy(bfrOut, "\033e", 2); break;     // 25h - cursor ON
                default:    moveOut = 0;                 break;
            }
        }; break;

        case 'l': {
            switch(v1) {
                case 7:     memcpy(bfrOut, "\033w", 2); break;     // 7l - wrap OFF
                case 25:    memcpy(bfrOut, "\033f", 2); break;     // 25l - cursor OFF
                default:    moveOut = 0;                 break;
            }
        }; break;

        case 'f':                               // f - move cursor to position
        case 'H': {                             // H - cursor home / cursor to position
            if(moveIn == 3) {                   // ESC + [ + H -- just home (0, 0)
                memcpy(bfrOut, "\033H", 2);
            } else {
                memcpy(bfrOut, "\033Y", 2);    // ESC + [ + row ; col + H - move cursor to position
                bfrOut[2] = ' ' + v1;
                bfrOut[3] = ' ' + v2;
                moveOut = 4;
            }
        }; break;

        case 'A':   // A - cursor up
        case 'B':   // B - cursor down
        case 'C':   // C - cursor forward (right)
        case 'D':   // D - cursor backward (left)
            vt100CursorMove(cmd, v1, moveIn, bfrOut, moveOut);
            break;

        case 's':   memcpy(bfrOut, "\033j", 2); break;    // save cursor position
        case 'u':   memcpy(bfrOut, "\033k", 2); break;    // restore cursor position

        case 'J': {         // J - erasing
            switch(v1) {
                case 0:     memcpy(bfrOut, "\033J", 2); break;     // Erases the screen from the current line down to the bottom of the screen.
                case 1:     memcpy(bfrOut, "\033d", 2); break;     // Erases the screen from the current line up to the top of the screen.
                case 2:     memcpy(bfrOut, "\033E", 2); break;     // Erases the screen with the background colour and moves the cursor to home.
                default:    moveOut = 0;    break;                  // no output = no moveOut
            }
        }; break;

        case 'K': {         // K - erasing
            switch(v1) {
                case 0:     memcpy(bfrOut, "\033K", 2); break;     // Erase to end of line
                case 1:     memcpy(bfrOut, "\033o", 2); break;     // Erase from beginning of line to cursor
                case 2:     memcpy(bfrOut, "\033l", 2); break;     // erase entire line
                default:    moveOut = 0;    break;                  // no output = no moveOut
            }
        }; break;

        case 'm':
            if((v2 == 39 && v3 == 49) || (v2 == 37 && v3 == 40)) {      // Foreground Default + Background Default OR Foreground White + Background Black
                memcpy(bfrOut, "\033q", 2);                            // Switch off inverse video text.
            } else if(v2 == 7 || v3 == 7 || (v2 == 30 && v3 == 47)) {   // negative (Swaps foreground and background colors) OR Foreground Black + Background White
                memcpy(bfrOut, "\033p", 2);                            // Switch on inverse video text
            } else {                            // rest is ignored, doesn't move the output pointer
                moveOut = 0;
            }

            break;

        // unknown commands
        default:
            dumpCmd("unknown ESC+[+x", cmd, bfrIn);
            moveOut = 0;        // no moving of output on unknown command
            break;
    }
}

int convertVT100toVT52(char *bfrIn, int cnt)
{
    char *bfrInStart = bfrIn;               // keep a copy of bfrIn at the start, we will copy converted string there
    static char tmp[READ_BUFFER_SIZE];      // where the filtered data will be built
    char* bfrOut = &tmp[0];                 // pointer to where the current new filtered data can be stored
    char* bfrInEnd = bfrIn + cnt;           // where the input data ends
    int moveIn, moveOut;

    // go through all the input buffer, find VT sequences, convert them, or just copy data if it's not a VT sequence
    while(bfrIn < bfrInEnd) {
        // not VT100 cmd, copy it
        if(*bfrIn != 27) {
            *bfrOut++ = *bfrIn++;
            continue;
        }

        if(bfrIn[1] != '[') {           // VT commands which are 'ESC + something'
            vt100shorter(bfrIn, bfrOut, moveIn, moveOut);
        } else {                        // VT commands which are 'ESC + [ + something'
            vt100longer(bfrIn, bfrOut, moveIn, moveOut);
        }

        // move forward in input and output buffer
        bfrIn += moveIn;
        bfrOut += moveOut;
    }

    char* bfrOutStart = &tmp[0];        // pointer to start of tmp (output) buffer
    int outLen = bfrOut - bfrOutStart;  // count of stored data = pointer after filtering - pointer at the start
    memcpy(bfrInStart, tmp, outLen);    // now copy the filtered data to original start of bfrIn
    bfrInStart[outLen] = 0;             // terminate string with zero
    return outLen;
}

void testConv(const char* vt100, const char* vt52)
{
    char test[128];
    strcpy(test, vt100);
    convertVT100toVT52(test, strlen(test));

    bool ok = strcmp(test, vt52) == 0;  // OK if everything matching

    if(!ok) {
        printf("Failure on: %s\n", vt100);

        int len = strlen(test);
        for(int i=0; i<len; i++) {
            printf("%02X - %c\n", test[i], test[i]);
        }
    }
}

int main(int argc, char *argv[])
{
    /* NOTE!
    When reading the vt100.raw file and testing conversions on it, please keep in mind that these commands start
    at position 0, and one line might contain additional command which might be incomplete. This is when you
    get message on screen like this: "unknown ESC+[+x: (0x0A)".
    This is expected and can be ignored (0x0A is new line character).
    */

    FILE* f = fopen("/tmp/vt100.raw", "rt");
    if(!f) {
        printf("\nCouldn't read input file, terminating.\n");
        return 1;
    }

    int lineNo = 0;
    while(!feof(f)) {
        char bfr[READ_BUFFER_SIZE];
        memset(bfr, 0, sizeof(bfr));
        char* res = fgets (bfr, sizeof(bfr), f);        // read line from raw stream

        if(!res) {
            continue;
        }

        int len = convertVT100toVT52(bfr, strlen(bfr));                  // convert individual lines

        if(len == -1) {
            VT100log(bfr, strlen(bfr));
        }
    }

    fclose(f);

    // some tests of expected conversions
    testConv("Normal\033[0;39;7mInverse\033[0;39;49mBack", "Normal\033pInverse\033qBack");  // inverse and normal colors
    testConv("This\033[Ais\033[2BSparta", "This\033Ais\033B\033BSparta");                   // move cursor 1x and 2x
    testConv("Let's go \033[fhome\033[H", "Let's go \033Hhome\033H");  // 'f' and 'H' without arguments
    testConv("Let's go \033[2;3fsomewhere\033[20;22H", "Let's go \033Y\042\043somewhere\033Y\064\066");  // 'f' and 'H' with new position
    testConv("Cursor Save\033[s and restore\033[u", "Cursor Save\033j and restore\033k");

    return 0;
}

void VT100log(char* bfr, int len)
{
    for(int i=0; i<len; i++) {
        printf("%02X ", bfr[i]);
    }

    printf(" -- ");

    for(int i=0; i<len; i++) {
        printf("%c", (bfr[i] >= 32) ? bfr[i] : '.');
    }

    printf("\n");
}
