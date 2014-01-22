#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "debug.h"
#include "utils.h"

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


	FILE *f = fopen("ce.log", "a+t");
	
	if(!f) {
		printf("%08d: ", Utils::getCurrentMs());
		vprintf(format, args);
		printf("\n");
		return;
	}

	fprintf(f, "%08d: ", Utils::getCurrentMs());
    vfprintf(f, format, args);
	fprintf(f, "\n");

    va_end(args);
	
	fclose(f);
}