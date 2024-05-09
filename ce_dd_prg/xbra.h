#ifndef XBRA_H
#define XBRA_H

#include <stdint.h>

typedef struct xbra
{
	uint32_t xbra_id;
	uint32_t app_id;
	void (*oldvec)();
} XBRA;

uint32_t unhook_xbra( uint16_t vecnum, uint32_t app_id );

#endif
