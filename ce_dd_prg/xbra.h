#ifndef XBRA_H
#define XBRA_H

#ifndef WORD
typedef short WORD;
#endif
#ifndef LONG
typedef long LONG;
#endif
#ifndef VOID
typedef void VOID;
#endif

typedef struct xbra
{
	LONG xbra_id;
	LONG app_id;
	VOID (*oldvec)();
} XBRA;

LONG unhook_xbra( WORD vecnum, LONG app_id );

#endif
