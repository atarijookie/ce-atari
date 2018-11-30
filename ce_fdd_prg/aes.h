#ifndef __AES_H__
#define __AES_H__

#include <gem.h>
#include <mt_gem.h>

#define MAX_STRING_LEN_COUNT    64

typedef struct {
    OBJECT *tree;           // pointer to the tree
    int16_t xdial, ydial, wdial, hdial; // dimensions and position of dialog

    BYTE maxStringLen[MAX_STRING_LEN_COUNT];    // holds max string length for string at specified index (or zero when length not found out yet)

    BYTE drawingDisabled;   // set to TRUE, when updating more things on screen and you want to do one whole redraw of dialog, not multiple partial redraws
} Dialog;

extern Dialog *cd;  // cd - pointer to Current Dialog, so we don't have to pass dialog pointer to functions

void redrawDialog(void);
void redrawObject(int16_t objId);
void setVisible(int objId, BYTE visible);
BYTE isSelected(int objIdx);
void selectButton(int btnIdx, BYTE select);
void enableButton(int btnIdx, BYTE enabled);
void setObjectString(int16_t objId, const char *newString);
void prepareDialog(void);
void showDialog(BYTE show);
void showErrorDialog(char *errorText);
void showComErrorDialog(void);

BYTE gem_init(void);
void gem_deinit(void);

#endif

