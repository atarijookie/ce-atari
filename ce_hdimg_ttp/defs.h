#ifndef _DEFS_H_
#define _DEFS_H_

uint8_t ce_acsiReadCommand(void);
uint8_t ce_acsiWriteBlockCommand(void);

uint8_t getKey(void);
void showComError(void);
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


#endif
