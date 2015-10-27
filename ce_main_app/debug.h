#ifndef _DEBUG_H_
#define _DEBUG_H_

#include "datatypes.h"

#define LOG_OFF         0
#define LOG_INFO        1       // info         - info which can be displayed when running at user's place
#define LOG_ERROR       2       // errors       - should be always visible, even to users
#define LOG_DEBUG       3       // debug info   - useful only to developers

typedef struct {
    BYTE    isInHandleAcsiCommand;
    DWORD   prevAcsiCmdTime;
    DWORD   thisAcsiCmdTime;   
} DebugVars;

class Debug
{
public:
	static void out(int logLevel, const char *format, ...);
	static void outBfr(BYTE *bfr, int count);

    static void printfLogLevelString(void);
    
    static void setOutputToConsole(void);
    static void setDefaultLogFile(void);
    static void setLogFile(char *path);
    
private:    
    static char logFilePath[128];
};

#endif

