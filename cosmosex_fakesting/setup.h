#ifndef _SETUP_H_
#define _SETUP_H_

// The Memory Manager
int16  KRinitialize (int32 size);
void  *KRmalloc (int32 size);
void   KRfree (void *mem_block);
int32  KRgetfree (int16 block_flag);
void  *KRrealloc (void *mem_block, int32 new_size);

#endif

