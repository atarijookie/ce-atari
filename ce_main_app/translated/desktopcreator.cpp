#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../debug.h"
#include "desktopcreator.h"

void DesktopCreator::createToFile(DesktopConfig *dc)
{
    char tmp[10240];
    int len = createToBuffer(tmp, 10240, dc);
    
    unlink("/tmp/configdrive/DESKTOP.INF");                 // delete old files
    unlink("/tmp/configdrive/NEWDESK.INF");
    
    FILE *f;
    if(dc->tosVersion < TOSVER205) {            // for TOS 1.xx
        Debug::out(LOG_DEBUG, "DesktopCreator::createToFile() -- will create /tmp/configdrive/DESKTOP.INF");
        f = fopen("/tmp/configdrive/DESKTOP.INF", "wt");
    } else {                                    // for TOS 2.xx and up
        Debug::out(LOG_DEBUG, "DesktopCreator::createToFile() -- will create /tmp/configdrive/NEWDESK.INF");
        f = fopen("/tmp/configdrive/NEWDESK.INF", "wt");
    }
    
    if(!f) {                                    // if failed to create file, quit
        Debug::out(LOG_DEBUG, "DesktopCreator::createToFile() -- failed to create file .INF file!");
        return;
    }
    
    fwrite(tmp, 1, len, f);                     // write content and close file    
    fclose(f);
}

int DesktopCreator::createToBuffer(char *bfr, int bfrSize, DesktopConfig *dc)
{
    Debug::out(LOG_DEBUG, "DesktopCreator::createToBuffer() -- creating DESKTOP.INF / NEWDESK.INF for TOS %d.%02x", dc->tosVersion >> 8, dc->tosVersion & 0xff);

    char b[10240];
    memset(b, 0, 10240);                                    // clear temp buffer
    
    memset(bfr, 0, bfrSize);                                // clear destination buffer
    
    char *p = b;                                            // start of the tmp buffer
    p = storeHeader(p, dc);
    p = storeWindowPositions(p, dc);
    
    if(dc->tosVersion >= TOSVER205) {
        p = storeExecutables(p, dc);
    }
    
    p = storeExistingDrives(p, dc);
    
    if(dc->tosVersion < TOSVER205) {
        p = storeExecutables(p, dc);
    }
    
    int len = p - b;                                        // calculate generated length of text
    int sz = (bfrSize > len) ? len : bfrSize;               // if enough space - copy all, otherwise copy just what can fit into bfr
    memcpy(bfr, b, sz);                                     // copy in the data
    
    return sz;
}

char *DesktopCreator::storeHeader(char *bfr, DesktopConfig *dc)
{
    char tmp[128];

    // some header here
    strcpy(bfr, "#a000000\r\n#b000000\r\n#c7770007000600070055200505552220770557075055507703111103\r\n#d                                             \r\n");
    bfr += strlen(bfr);

    // then some config with dc->settingsResolution settings
    // fix the "med res" 1.06 TOS bug by outputing 3 (high res) instead of 2 (med res)
    int len = snprintf(tmp, sizeof(tmp), "#E 98 %1X%1X\r\n",
                       (dc->tosVersion >= TOSVER102) ? 1 : 0, // Enable Blitter
                       (dc->tosVersion == TOSVER106 && dc->settingsResolution == 2) ? 3 : dc->settingsResolution);
    memcpy(bfr, tmp, len + 1);
    bfr += len;
    
    return bfr;
}

char *DesktopCreator::storeWindowPositions(char *bfr, DesktopConfig *dc)
{
    char tmp[128];
    int n;
    
    if(dc->tosVersion < TOSVER205) {        // store 4 windows in older TOS
        n = 4;
    } else {                            // store 8 windows in newer TOS
        n = 8;
    }
    
    for(int i=0; i<n; i++) {
        sprintf(tmp, "#W 00 00 08 01 1E 15 00 @\r\n");
        strcpy(bfr, tmp);
        bfr += strlen(tmp);
    }
    
    return bfr;
}

char *DesktopCreator::storeExecutables(char *bfr, DesktopConfig *dc)
{
    if(dc->tosVersion < TOSVER205) {
        strcpy(bfr, "#F FF 04   @ *.*@ \r\n#D FF 01   @ *.*@ \r\n#G 03 FF   *.APP@ @ \r\n#G 03 FF   *.PRG@ @ \r\n#F 03 04   *.TOS@ @ \r\n#P 03 04   *.TTP@ @ \r\n");
        bfr += strlen(bfr);
        
        bfr = storeFloppyImageLauncher(bfr, dc);

        bfr = storeMediaPlayers(bfr, dc);
        
        *bfr = 0x1a;
        bfr++;
    } else {
        strcpy(bfr, "#N FF 04 000 @ *.*@ @ \r\n#D FF 01 000 @ *.*@ @ \r\n#G 03 FF 000 *.APP@ @ @ \r\n#G 03 FF 000 *.PRG@ @ @ \r\n#F 03 04 000 *.TOS@ @ @ \r\n#P 03 04 000 *.TTP@ @ @ \r\n#Y 03 04 000 *.GTP@ @ @ \r\n");
        bfr += strlen(bfr);

        bfr = storeFloppyImageLauncher(bfr, dc);
        
        bfr = storeMediaPlayers(bfr, dc);
    }   
    
    return bfr;
}

char *DesktopCreator::storeFloppyImageLauncher(char *bfr, DesktopConfig *dc)
{
    char tmp[64];
    // add app for opening .ST images
    sprintf(tmp, "#G 03 04   %c:\\CE_FDD.TTP@ *.ST@ \r\n", 'A' + dc->configDrive);
    strcpy(bfr, tmp);
    bfr += strlen(bfr);

    // add app for opening .MSA images
    sprintf(tmp, "#G 03 04   %c:\\CE_FDD.TTP@ *.MSA@ \r\n", 'A' + dc->configDrive);
    strcpy(bfr, tmp);
    bfr += strlen(bfr);

    return bfr;
}

char *DesktopCreator::storeMediaPlayers(char *bfr, DesktopConfig *dc)
{
	char tmp[128];
	int len;
	len = snprintf(tmp, sizeof(tmp),
	               "#P 03 04   %c:\\STEAUPLY.TTP@ *.AU@ \r\n", 'A' + dc->configDrive);
	memcpy(bfr, tmp, len+1);
	bfr += len;
	len = snprintf(tmp, sizeof(tmp),
	               "#P 03 04   %c:\\CEMEDIAP.TTP@ *.MP3@ \r\n", 'A' + dc->configDrive);
	memcpy(bfr, tmp, len+1);
	bfr += len;
	return bfr;
}

char *DesktopCreator::storeExistingDrives(char *bfr, DesktopConfig *dc)
{
    char tmp[128];
    int x = 0, y = 0;
    int cols = (dc->settingsResolution == 1) ? 4 : 8;       // LOW res: 4 cols for icons, MID & HI res: 8 cols for icons
    int rows = (dc->settingsResolution <  3) ? 4 : 8;       // LOW and MID res: 4 rows, HI res: 8 rows for icons
    
    int   driveIcon = 0;
    const char *driveName = "derp";
    
    for(int i=0; i<16; i++) {                               // go through all the drives
        if((dc->drivesAll & (1 << i)) == 0) {               // drive not active? skip
            continue;
        }
    
        //----------
        // get drive name
        if(i < 2) {                                         // it's a floppy?
            driveName = "FLOPPY DISK";
            driveIcon = 0x09;
        } else {                                            // it's not a floppy, but...
            if((dc->translatedDrives & (1 << i)) == 0) {    // not translated drive?
                driveName = "HARD DISK";
                driveIcon = 0x0b;
            } else {                                        // it's a translated drive!
                if(i == dc->configDrive) {                  // it's a config drive
                    driveName = "CONFIG DRIVE";
                    driveIcon = 0x07;
                } else if(i == dc->sharedDrive) {           // it's a shared drive
                    driveName = "SHARED DRIVE";
                    driveIcon = 0x06;
                } else {                                    // it's a usb drive
                    driveName = "USB DRIVE";
                    driveIcon = 0x08;
                }
            }
        }

        //----------
        // now place the drive icons in desktop
        char driveLetter = 'A' + i;
        
        if(dc->tosVersion < TOSVER205) {                // for TOS 1.xx - no specific drive icons
            driveIcon = 0;
        }
        
        sprintf(tmp, "#M %02X %02X %02X FF %c %s@ @ \r\n", x, y, driveIcon, driveLetter, driveName);
        strcpy(bfr, tmp);
        bfr += strlen(tmp);
        
        x++;                                            // move to next column
        if(x >= cols) {                                 // going beyond last column? new line!
            x = 0;
            y ++;
        }
    }

    //---------
    // now place trash can on the desktop
    sprintf(tmp, "#T 00 %02X 02 FF   TRASH@ @ \r\n", rows - 1);
    strcpy(bfr, tmp);
    bfr += strlen(tmp);
    
    return bfr;
}

