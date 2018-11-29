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
#include "aes.h"
#include "CE_FDD.H"

Dialog *cd;         // cd - pointer to current dialog, so we don't have to pass dialog pointer to functions

void redrawObject(int16_t objId)
{
    if(cd->drawingDisabled) {       // if drawing now disabled, don't do anything
        return;
    }

    if(objId < 0 || objId > cd->tree->ob_tail) {    // index too small or too big? quit
        return;
    }

    OBJECT *obj = &cd->tree[objId];

    int16_t ox, oy;
    objc_offset(cd->tree, objId, &ox, &oy);          // get current screen coordinates of object

    objc_draw(cd->tree, ROOT, MAX_DEPTH, ox - 2, oy - 2, obj->ob_width + 4, obj->ob_height + 4); // draw object tree, but clip only to text position and size + some pixels more around to hide button completely
}

void redrawDialog(void)
{
    if(cd->drawingDisabled) {       // if drawing now disabled, don't do anything
        return;
    }

    if(!cd || !cd->tree) {          // no current dialog or tree?
        return;
    }

    objc_draw(cd->tree, ROOT, MAX_DEPTH, cd->xdial, cd->ydial, cd->wdial, cd->hdial);
}

void setVisible(int objId, BYTE visible)
{
    if(objId < 0 || objId > cd->tree->ob_tail) {    // index too small or too big? quit
        return;
    }

    // object was visible, if the flag HIDETREE is zero
    BYTE wasVisible = (cd->tree[ objId ].ob_flags & OF_HIDETREE) == 0;

    if(visible) {   // show? remove flag
        cd->tree[ objId ].ob_flags &= (~OF_HIDETREE);
    } else {        // hide? add flag
        cd->tree[ objId ].ob_flags |= OF_HIDETREE;
    }

    if((wasVisible && !visible) || (!wasVisible && visible)) {  // if visibility changed, redraw
        redrawObject(objId);
    }
}

BYTE isSelected(int objIdx)
{
    if(objIdx < 0 || objIdx > cd->tree->ob_tail) {    // index too small or too big? quit
        return FALSE;
    }

    OBJECT *obj = &cd->tree[objIdx];
    return (obj->ob_state & OS_SELECTED) != 0;  // is selected if selected bit not zero
}

void selectButton(int btnIdx, BYTE select)
{
    if(btnIdx < 0 || btnIdx > cd->tree->ob_tail) {    // index too small or too big? quit
        return;
    }

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
    if(btnIdx < 0 || btnIdx > cd->tree->ob_tail) {    // index too small or too big? quit
        return;
    }

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
    if(objId < 0 || objId > cd->tree->ob_tail) {    // index too small or too big? quit
        return;
    }

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
    if(!cd || !cd->tree) {  // if current dialog not set or tree not set, quit
        return;
    }

    if(show) {  // on show
        form_center(cd->tree, &cd->xdial, &cd->ydial, &cd->wdial, &cd->hdial);  // center object
        form_dial(0, 0, 0, 0, 0, cd->xdial, cd->ydial, cd->wdial, cd->hdial);   // reserve screen space for dialog
        objc_draw(cd->tree, ROOT, MAX_DEPTH, cd->xdial, cd->ydial, cd->wdial, cd->hdial);  // draw object tree
    } else {    // on hide
        form_dial (3, 0, 0, 0, 0, cd->xdial, cd->ydial, cd->wdial, cd->hdial);      // release screen space
    }
}

void showErrorDialog(char *errorText)
{
    if(!cd) {           // don't have current dialog specified? error to console
        (void) Cconws("\r");
        (void) Cconws(errorText);
        (void) Cconws("\r\n");
        Cnecin();
        return;
    }

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

BYTE gem_init(void)
{
    int16_t work_in[11], i, work_out[64];
    gl_apid = appl_init();

    if (gl_apid == -1) {
        (void) Cconws("appl_init() failed\r\n");
        return FALSE;
    }

    wind_update(BEG_UPDATE);
    graf_mouse(HOURGLASS, 0);

    int16_t res = rsrc_load("CE_FDD.RSC");
    graf_mouse(ARROW, 0);

    if(!res) {
        (void) Cconws("rsrc_load() failed\r\n");
        return FALSE;
    }

    for(i=0; i<10; i++) {
        work_in[i]= i;
    }

    work_in[10] = 2;

    int16_t gem_handle, vdi_handle;
    int16_t gl_wchar, gl_hchar, gl_wbox, gl_hbox;

    gem_handle = graf_handle(&gl_wchar, &gl_hchar, &gl_wbox, &gl_hbox);
    vdi_handle = gem_handle;
    v_opnvwk(work_in, &vdi_handle, work_out);

    if (vdi_handle == 0) {
        (void) Cconws("v_opnvwk() failed\r\n");
        return FALSE;
    }

    return TRUE;
}

void gem_deinit(void)
{
    rsrc_free();            // free resource from memory
    appl_exit();
}
