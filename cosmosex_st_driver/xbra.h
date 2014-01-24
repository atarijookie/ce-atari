#ifndef XBRA_H
#define XBRA_H

typedef short WORD;
typedef long LONG;
typedef void VOID;

// this value represents string XBRA
#define XBRA_TAG	0x58425241

// this value represents string CEDD
#define XBRA_ID		0x43454444

typedef struct xbra
{
	LONG xbra_id;
	LONG app_id;
	VOID (*oldvec)();
} XBRA;

LONG unhook_xbra( WORD vecnum, LONG app_id );

#endif

