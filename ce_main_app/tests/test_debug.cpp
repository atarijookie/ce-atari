// vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab
// debug output implementation for tests binaries
#include <stdio.h>
#include <stdarg.h>
#include "../debug.h"

char Debug::logFilePath[128];

void Debug::setOutputToConsole(void)
{
}

void Debug::setDefaultLogFile(void)
{
}

void Debug::setLogFile(const char *path)
{
}

void Debug::printfLogLevelString(void)
{
	printf("TEST Debug output\n");
}

void Debug::out(int logLevel, const char *format, ...)
{
    va_list args;
    va_start(args, format);

    vprintf(format, args);
    printf("\n");

    va_end(args);
}

void Debug::outBfr(BYTE *bfr, int count)
{
    printf("outBfr - %d bytes\n", count);

    int i, j;

    int rows = (count / 16) + (((count % 16) == 0) ? 0 : 1);

    for(i=0; i<rows; i++) {
        int ofs = i * 16;
        printf("        ");//some indentation

        for(j=0; j<16; j++) {
            if((ofs + j) < count) {
                printf("%02x ", bfr[ofs + j]);
            } else {
                printf("   ");
            }
        }

        printf("| ");

        for(j=0; j<16; j++) {
            char v = bfr[ofs + j];
            v = (v >= 32 && v <= 126) ? v : '.';

            if((ofs + j) < count) {
                printf("%c", v);
            } else {
                printf(" ");
            }
        }

        printf("\n");
    }
}
