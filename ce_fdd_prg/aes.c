#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <gem.h>
#include <mt_gem.h>

#include <stdint.h>
#include <stdio.h>

#include "stdlib.h"
#include "acsi.h"
#include "main.h"
#include "hostmoddefs.h"
#include "keys.h"
#include "defs.h"
#include "CE_FDD.H"

typedef struct {
    OBJECT *tree;   // pointer to the tree
    int16_t xdial, ydial, wdial, hdial; // dimensions and position of dialog

    #define MAX_STRING_LEN_COUNT    64
    BYTE maxStringLen[MAX_STRING_LEN_COUNT];    // holds max string length for string at specified index (or zero when length not found yet)
} Dialog;

Dialog *cd;         // cd - pointer to current dialog, so we don't have to pass dialog pointer to functions

void redrawObject(int16_t objId)
{
    OBJECT *obj = &cd->tree[objId];

    int16_t ox, oy;
    objc_offset(cd->tree, objId, &ox, &oy);          // get current screen coordinates of object

    objc_draw(cd->tree, ROOT, MAX_DEPTH, ox - 2, oy - 2, obj->ob_width + 4, obj->ob_height + 4); // draw object tree, but clip only to text position and size + some pixels more around to hide button completely
}

void selectButton(int btnIdx, BYTE select)
{
    OBJECT *btn = &cd->tree[btnIdx];

    if(select) {
        btn->ob_state = btn->ob_state | OS_SELECTED;        // add SELECTED flag
    } else {
        btn->ob_state = btn->ob_state & (~OS_SELECTED);     // remove SELECTED flag
    }

    redrawObject(btnIdx);
}

void enableButton(int btnIdx, BYTE enabled)
{
    OBJECT *btn = &cd->tree[btnIdx];

    if(enabled) {   // if enabled, remove DISABLED flag
        btn->ob_state = btn->ob_state & (~OS_DISABLED);
    } else {        // if disabled, add DISABLED flag
        btn->ob_state = btn->ob_state | OS_DISABLED;
    }

    redrawObject(btnIdx);
}

void setObjectString(int16_t objId, const char *newString)
{
    OBJECT *obj = &cd->tree[objId];
    int maxLen = 0;                             // no allowed length until we find out

    if(objId >= MAX_STRING_LEN_COUNT) {         // if the cd->maxStringLen[] is too small for this item index, show warning (development usage only - shouldn't appear in a good app)
        (void) Clear_home();
        (void) Cconws("setObjectString -- cd->maxStringLen array too small!\r\n");
    } else {
        if(cd->maxStringLen[objId] == 0) {      // if the string length wasn't determined yet, find it out
            cd->maxStringLen[objId] = strlen(obj->ob_spec.free_string);
        }

        maxLen = cd->maxStringLen[objId];       // store what we think is the maximum length
    }

    int newLen = strlen(newString);             // get length of what we're trying to set

    if(newLen <= maxLen) {                                  // new string shorter or exact as it should be?
        strcpy(obj->ob_spec.free_string, newString);        // copy in whole string
    } else {                                                // new string longer than it should be? clip it
        memcpy(obj->ob_spec.free_string, newString, maxLen);// copy part of the string
        obj->ob_spec.free_string[maxLen] = 0;               // zero terminate string
    }

    redrawObject(objId);                    // redraw it
}

void showDialog(BYTE show)
{
    if(show) {  // on show
        form_center(cd->tree, &cd->xdial, &cd->ydial, &cd->wdial, &cd->hdial);       // center object
        form_dial(0, 0, 0, 0, 0, cd->xdial, cd->ydial, cd->wdial, cd->hdial);       // reserve screen space for dialog
        objc_draw(cd->tree, ROOT, MAX_DEPTH, cd->xdial, cd->ydial, cd->wdial, cd->hdial);  // draw object tree
    } else {    // on hide
        form_dial (3, 0, 0, 0, 0, cd->xdial, cd->ydial, cd->wdial, cd->hdial);      // release screen space
    }
}

void showErrorDialog(char *errorText)
{
    showDialog(FALSE);   // hide dialog

    char tmp[256];
    strcpy(tmp, "[3][");                // STOP icon
    strcat(tmp, errorText);             // error text
    strcat(tmp, "][ OK ]");             // OK button

    form_alert(1, tmp);                 // show alert dialog, 1st button as default one, with text and buttons in tmp[]

    showDialog(TRUE);   // show dialog
}

void showComErrorDialog(void)
{
    showErrorDialog("Error in CosmosEx communication!");
}
