#ifndef _DEFS_H_
#define _DEFS_H_

uint8_t ce_acsiReadCommand(void);
uint8_t ce_acsiWriteBlockCommand(void);

void intToStr(int val, char *str);
void removeLastPartUntilBackslash(char *str);
uint8_t getLowestDrive(void);

#define GOTO_POS        "\33Y"
#define Goto_pos(x,y)   ((void) Cconws(GOTO_POS),  (void) Cconout(' ' + y), (void) Cconout(' ' + x))

#define SIZE64K     (64*1024)

typedef struct {
    uint8_t isSet;
    char path[256];
} TDestDir;

typedef struct {
    uint8_t encoding;              // is the RPi encoding the image or being idle?
    uint8_t doWeHaveStorage;       // do we have storage for floppy images?
    uint8_t prevDoWeHaveStorage;   // previous value of doWeHaveStorage

    uint8_t downloadCount;         // how many files are now being downloaded?
    uint8_t prevDownloadCount;     // previous value of downloadCount
} Status;

#endif
