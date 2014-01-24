#ifndef _DEBUG_H_
#define _DEBUG_H_

#include "datatypes.h"

class Debug
{
public:
	static void out(const char *format, ...);
	static void outBfr(BYTE *bfr, int count);
};

#endif

