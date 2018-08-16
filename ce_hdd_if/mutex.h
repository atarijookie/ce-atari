#ifndef _MUTEX_H_
#define _MUTEX_H_
/*
Poor mans mutex
*/
#include "global.h"

typedef volatile BYTE mutex;

inline BYTE mutex_trylock(mutex *mtx);
inline void mutex_unlock(mutex *mtx);

#endif
