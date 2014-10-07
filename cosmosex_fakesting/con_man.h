#ifndef _CON_MAN_H_
#define _CON_MAN_H_

int16      /* cdecl */  CNkick (int16 connec);
int16      /* cdecl */  CNbyte_count (int16 connec);
int16      /* cdecl */  CNget_char (int16 connec);
NDB *      /* cdecl */  CNget_NDB (int16 connec);
int16      /* cdecl */  CNget_block (int16 connec, void *buffer, int16 length);
CIB *      /* cdecl */  CNgetinfo (int16 connec);
int16      /* cdecl */  CNgets (int16 connec, char *buffer, int16 length, char delimiter);

#endif