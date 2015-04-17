#ifndef _DESKTOPCREATOR_H_
#define _DESKTOPCREATOR_H_

#include "../datatypes.h"

#define TOSVER100   0x0100
#define TOSVER102   0x0102
#define TOSVER104   0x0104
#define TOSVER106   0x0106
#define TOSVER162   0x0162
#define TOSVER205   0x0205
#define TOSVER206   0x0206
#define TOSVER300   0x0300
#define TOSVER400   0x0400

typedef struct {
    WORD tosVersion;
    WORD resolution;            // ST video resolution

    DWORD drivesAll;            // bitmap of all drives available
    DWORD translatedDrives;     // bitmap of drives which are translated
    BYTE  configDrive;          // index of config drive
    BYTE  sharedDrive;          // index of shared drive
} DesktopConfig;

class DesktopCreator {
public: 
    static void  createToFile(DesktopConfig *dc);
    static int   createToBuffer(char *bfr, int bfrSize, DesktopConfig *dc);

private:    
    static char *storeHeader         (char *bfr, DesktopConfig *dc);
    static char *storeExecutables    (char *bfr, DesktopConfig *dc);
    static char *storeWindowPositions(char *bfr, DesktopConfig *dc);
    static char *storeExistingDrives (char *bfr, DesktopConfig *dc);

};

#endif

