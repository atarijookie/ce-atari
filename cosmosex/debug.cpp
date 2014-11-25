#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "debug.h"
#include "utils.h"

#define LOG_FILE		"/var/log/ce.log"
DWORD prevLogOut;

extern BYTE g_logLevel;                     // current log level 
       BYTE g_outToConsole;

char Debug::logFilePath[128];

void Debug::setOutputToConsole(void)
{
    g_outToConsole = 1;
}

void Debug::setDefaultLogFile(void)
{
    setLogFile((char *) LOG_FILE);
}

void Debug::setLogFile(char *path)
{
    strcpy(Debug::logFilePath, path);
}
    
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

	FILE *f;

    if(g_outToConsole) {                    // should log to console? f is null
        f = NULL;
    } else {                                // log to file? open the file
        f = fopen(logFilePath, "a+t");
    }
	
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

	FILE *f = fopen(logFilePath, "a+t");
	
	if(!f) {
		return;
	}

	fprintf(f, "%08d: outBfr - %d bytes\n", Utils::getCurrentMs(), count);

    int i, j;
    
    int rows = (count / 16) + (((count % 16) == 0) ? 0 : 1);
     
	for(i=0; i<rows; i++) {
        int ofs = i * 16;
        
        for(j=0; j<16; j++) {
            if((ofs + j) < count) {
                fprintf(f, "%02x ", bfr[ofs + j]);
            } else {
                fprintf(f, "   ");
            }
        }

        fprintf(f, "| ");

        for(j=0; j<16; j++) {
            char v = bfr[ofs + j];
            v = (v >= 32 && v <= 126) ? v : '.';
            
            if((ofs + j) < count) {
                fprintf(f, "%c", v);
            } else {
                fprintf(f, " ");
            }
        }
        
        fprintf(f, "\n");
    }

	fprintf(f, "\n");
	fclose(f);
}

