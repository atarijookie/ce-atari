//--------------------------------------------------

#include <stdint.h>
#include <stdio.h>

#include <gem.h>
#include <mt_gem.h>

#include "../ce_hdd_if/stdlib.h"
#include "../ce_hdd_if/hdd_if.h"

//--------------------------------------------------
// following code is used to define a simple dialog for showing progress of scan using GEM
unsigned char SCAN_RSC[] = {
  0x00, 0x00, 0x00, 0x28, 0x00, 0x88, 0x00, 0x88, 0x00, 0x88, 0x00, 0x88,
  0x00, 0x88, 0x00, 0xb8, 0x00, 0xb8, 0x00, 0x24, 0x00, 0x04, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xb8,
  0x00, 0x00, 0x00, 0x28, 0xff, 0xff, 0x00, 0x01, 0x00, 0x03, 0x00, 0x14,
  0x00, 0x00, 0x00, 0x10, 0x00, 0x02, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x16, 0x00, 0x06, 0x00, 0x02, 0xff, 0xff, 0xff, 0xff, 0x00, 0x1c,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0x00, 0x01, 0x00, 0x01,
  0x00, 0x14, 0x00, 0x01, 0x00, 0x03, 0xff, 0xff, 0xff, 0xff, 0x00, 0x1c,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9d, 0x00, 0x01, 0x00, 0x02,
  0x00, 0x14, 0x00, 0x01, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x00, 0x1a,
  0x00, 0x27, 0x00, 0x00, 0x00, 0x00, 0x00, 0xb2, 0x00, 0x07, 0x00, 0x04,
  0x00, 0x08, 0x00, 0x01, 0x4c, 0x6f, 0x6f, 0x6b, 0x69, 0x6e, 0x67, 0x20,
  0x66, 0x6f, 0x72, 0x20, 0x43, 0x6f, 0x73, 0x6d, 0x6f, 0x73, 0x45, 0x78,
  0x00, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0x51, 0x75,
  0x69, 0x74, 0x00, 0x00
};
unsigned int SCAN_RSC_len = 184;

#define SCAN             0   /* Form/Dialog-box */
#define ROOTBOX          0   /* BOX in tree SCAN */
#define TITLE            1   /* STRING in tree SCAN */
#define STATUS           2   /* STRING in tree SCAN */
#define BTNQUIT          3   /* BUTTON in tree SCAN */

int maxStatusLen = 0;       // maximu length of status
OBJECT *scanDialogTree;     // this gets filled by getScanDialogTree() and then used when scanning

OBJECT *getScanDialogTree(void)
{
    RSHDR *rsc = (RSHDR *) SCAN_RSC;
    OBJECT *tree = (OBJECT *) (SCAN_RSC + rsc->rsh_object);     // pointer to first obj

    int i;
    for(i=0; i<rsc->rsh_nobs; i++) {                    // go through all the objects
        if(tree[i].ob_type == G_STRING || tree[i].ob_type == G_BUTTON) {  // if it's string or button, fix pointer to its text
            char *p = (char *) SCAN_RSC;                // pointer to start of RSC file
            p += (int) tree[i].ob_spec.free_string;     // add value from ob_spec, which is currently offset
            tree[i].ob_spec.free_string = (char *) p;   // store pointer back to ob_spec, which points now to the string
        }

        rsrc_obfix(tree, i);                            // convert coordinates from character-based to pixel-based
    }

    maxStatusLen = strlen(tree[STATUS].ob_spec.free_string);    // find out maximum status length
    scanDialogTree = tree;  // store it here, will use it then
    return tree;            // return pointer to dialog tree
}

void showStatusInGem(char *status)
{
    if(!scanDialogTree) {           // no dialog? quit
        return;
    }

    int newLen = strlen(status);

    if(newLen <= maxStatusLen) {                                        // new string shorter or exact as it should be?
        strcpy(scanDialogTree[STATUS].ob_spec.free_string, status);               // copy in whole string
    } else {                                                            // new string longer than it should be? clip it
        memcpy(scanDialogTree[STATUS].ob_spec.free_string, status, maxStatusLen); // copy part of the string
        scanDialogTree[STATUS].ob_spec.free_string[maxStatusLen] = 0;             // zero terminate string
    }

    int16_t ox, oy;
    OBJECT *obj = &scanDialogTree[STATUS];                  // pointer to status string object
    objc_offset(scanDialogTree, STATUS, &ox, &oy);          // get current screen coordinates of object
    objc_draw(scanDialogTree, ROOT, MAX_DEPTH, ox - 2, oy - 2, obj->ob_width + 4, obj->ob_height + 4); // draw object tree, but clip only to text position and size + some pixels more around to hide button completely
}

