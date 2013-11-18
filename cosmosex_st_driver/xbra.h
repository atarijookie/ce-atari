#ifndef XBRA_H
#define XBRA_H

typedef short WORD;
typedef long LONG;
typedef void VOID;

typedef struct xbra
{
	LONG xbra_id;
	LONG app_id;
	VOID (*oldvec)();
} XBRA;

LONG unhook_xbra( WORD vecnum, LONG app_id );

#endif

