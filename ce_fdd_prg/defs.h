#ifndef _DEFS_H_
#define _DEFS_H_

BYTE ce_acsiReadCommand(void);
BYTE ce_acsiWriteBlockCommand(void);

BYTE getKey(void);
BYTE getKeyIfPossible(void);
void showComError(void);
void showError(const char *error);
void intToStr(int val, char *str);
void removeLastPartUntilBackslash(char *str);
BYTE getLowestDrive(void);

#define GOTO_POS        "\33Y"
#define Goto_pos(x,y)   ((void) Cconws(GOTO_POS),  (void) Cconout(' ' + y), (void) Cconout(' ' + x))

#define SIZE64K     (64*1024)

typedef struct {
    BYTE isSet;
    char path[256];
} TDestDir;


#endif
