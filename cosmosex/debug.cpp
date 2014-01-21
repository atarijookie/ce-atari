#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "debug.h"

void Debug::out(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    vprintf(format, args);
	printf("\n");

    va_end(args);
}
