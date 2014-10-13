#ifndef _CON_MAN_H_
#define _CON_MAN_H_

//-------------------------------------
// connection info function
int16 CNkick        (int16 handle);
CIB * CNgetinfo     (int16 handle);
int16 CNbyte_count  (int16 handle);
//-------------------------------------
// data retrieval functions
NDB * CNget_NDB     (int16 handle);
int16 CNget_block   (int16 handle, void *buffer, int16 length);
int16 CNgets        (int16 handle, char *buffer, int16 length, char delimiter);
int16 CNget_char    (int16 handle);

//-------------------------------------
// helper functions
void update_con_info(void);

void handles_init(void);
int  handles_got(int16 h, int *index);

//-------------------------------------

#endif