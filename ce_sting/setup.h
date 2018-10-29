// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
#ifndef _SETUP_H_
#define _SETUP_H_

// The Memory Manager

// these should be only called from ST apps (cdecl calling convention)
void  *KRmalloc         (int32 size);
void   KRfree           (void *mem_block);
int32  KRgetfree        (int16 block_flag);
void  *KRrealloc        (void *mem_block, int32 new_size);
char  *get_error_text   (int16 error_code);

// these should be only called from this driver (gcc calling convention)
int16  KRinitialize         (int32 size);
void  *KRmalloc_internal    (int32 size);
void   KRfree_internal      (void *mem_block);
int32  KRgetfree_internal   (int16 block_flag);
void  *KRrealloc_internal   (void *mem_block, int32 new_size);

#endif
