#ifndef _CON_MAN_H_
#define _CON_MAN_H_

void handles_init(void);
int  handles_got(int16 h);

int16 CNkick        (int16 handle);
int16 CNbyte_count  (int16 handle);
int16 CNget_char    (int16 handle);
NDB * CNget_NDB     (int16 handle);
int16 CNget_block   (int16 handle, void *buffer, int16 length);
CIB * CNgetinfo     (int16 handle);
int16 CNgets        (int16 handle, char *buffer, int16 length, char delimiter);

#endif