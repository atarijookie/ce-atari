#ifndef _DEBUG_H_
#define _DEBUG_H_

#include "datatypes.h"

#define LOG_OFF         0
#define LOG_ERROR       1
#define LOG_INFO        2
#define LOG_DEBUG       3

class Debug
{
public:
	static void out(int logLevel, const char *format, ...);
	static void outBfr(BYTE *bfr, int count);

    static void printfLogLevelString(void);
};

#endif

