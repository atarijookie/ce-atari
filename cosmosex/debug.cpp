#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "debug.h"
#include "utils.h"

#define LOG_FILE		"/var/log/ce.log"
DWORD prevLogOut;

BYTE g_logLevel = LOG_ERROR;                // current log level 

void Debug::printfLogLevelString(void)
{
    printf("\nLog level: ");

    switch(g_logLevel) {
        case LOG_OFF:   printf("OFF"); break;
        case LOG_ERROR: printf("ERROR"); break;
        case LOG_INFO:  printf("INFO"); break;
        case LOG_DEBUG: printf("DEBUG"); break;
        default:        printf("unknown!"); break;
    }

    printf("\n\n");
}

void Debug::out(int logLevel, const char *format, ...)
{
    if(logLevel > g_logLevel) {             // if this log is higher than allowed, don't do this
        return;
    }

    va_list args;
    va_start(args, format);

	FILE *f = fopen(LOG_FILE, "a+t");
	
	if(!f) {
		printf("%08d: ", Utils::getCurrentMs());
		vprintf(format, args);
		printf("\n");
		return;
	}

	DWORD now = Utils::getCurrentMs();
	DWORD diff = now - prevLogOut;
	prevLogOut = now;
	
	fprintf(f, "%08d\t%08d\t ", now, diff);
    vfprintf(f, format, args);
	fprintf(f, "\n");
	fclose(f);

    va_end(args);
}

void Debug::outBfr(BYTE *bfr, int count)
{
    if(g_logLevel < LOG_DEBUG) {              // if we're not in debug log level, don't do this
        return;
    }

	FILE *f = fopen(LOG_FILE, "a+t");
	
	if(!f) {
		return;
	}

	fprintf(f, "%08d: ", Utils::getCurrentMs());

	for(int i=0; i<count; i++) {
		if((i % 16) == 0) {
			fprintf(f, "\n");
		}

		fprintf(f, "%02x ", bfr[i]);
	}

	fprintf(f, "\n");
	fclose(f);
}
