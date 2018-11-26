#ifndef __AES_H__
#define __AES_H__

#include <gem.h>
#include <mt_gem.h>

typedef struct {
    OBJECT *tree;   // pointer to the tree
    int16_t xdial, ydial, wdial, hdial; // dimensions and position of dialog
} Dialog;

extern Dialog *cd;  // cd - pointer to Current Dialog, so we don't have to pass dialog pointer to functions

void selectButton(int btnIdx, BYTE select);
void enableButton(int btnIdx, BYTE enabled);
void setObjectString(int16_t objId, const char *newString);
void showDialog(BYTE show);
void showErrorDialog(char *errorText);
void showComErrorDialog(void);

#endif

