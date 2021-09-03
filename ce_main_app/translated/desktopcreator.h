#ifndef _DESKTOPCREATOR_H_
#define _DESKTOPCREATOR_H_

#include <string>

#include <stdint.h>
#include "translateddisk.h"

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
    uint16_t tosVersion;
    uint16_t currentResolution;     // ST video resolution -- what ST says it currently has
    uint16_t settingsResolution;    // ST video resolution -- what is in the user settings

    uint32_t drivesAll;            // bitmap of all drives available
    uint32_t translatedDrives;     // bitmap of drives which are translated
    uint8_t  configDrive;          // index of config drive
    uint8_t  sharedDrive;          // index of shared drive
    std::string label[MAX_DRIVES];
    
    bool  sdNoobEnabled;        // is SD NOOB enabled for this session?
    uint32_t sdNoobSizeSectors;    // size of SD NOOB partition
    int   sdNoobDriveNumber;    // TOS drive # for SD NOOB
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
    
    static char *storeFloppyImageLauncher(char *bfr, DesktopConfig *dc);
    static char *storeMediaPlayers(char *bfr, DesktopConfig *dc);
};

#endif

