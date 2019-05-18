#ifndef _MUTEX_H_
#define _MUTEX_H_
/*
Poor mans mutex
*/
#include "global.h"

typedef volatile BYTE mutex;

//1 if lock could be aquired, 0 if it was already locked
#define	mutex_trylock(arg1)	\
__extension__							\
({	register BYTE retv __asm__("d0");	\
	__asm__ volatile					\
	(									\
		"movl	a0,-(a7)\n\t"				\
		"movl	%1,a0\n\t"				\
		"movq 	#1,d0\n\t"				\
		"tas	(a0)\n\t"			/* atomic! take that, Amiga! */	\
		"beq	.label\n\t"			\
		"movq 	#0,d0\n\t"			\
		".label: "			\
		"movl	(a7)+,a0\n\t"				\
		/* end of code */				\
	:	"=r"	(retv)		/* out */	\
	:	"r"		(arg1)		/* in */	\
	:	__CLOBBER_RETURN("d0") "a0"	\
	);									\
	retv;								\
})

static inline void mutex_unlock(mutex *mtx)
{
	*mtx=0;
}
#endif
