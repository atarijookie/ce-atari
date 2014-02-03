#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "debug.h"
#include "utils.h"

#define LOG_FILE		"ce.log"
DWORD prevLogOut;

/*
void Debug::out(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    vprintf(format, args);
	printf("\n");

    va_end(args);
}
*/

void Debug::out(const char *format, ...)
{
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