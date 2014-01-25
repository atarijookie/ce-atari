#ifndef _BIOS_H_
#define _BIOS_H_

int32_t custom_mediach( void *sp );
int32_t custom_drvmap ( void *sp );
int32_t custom_getbpb ( void *sp );

void updateCeDrives(void);
void updateCeMediach(void);

#define GET_MEDIACH		0xffff
WORD getSetCeMediach(WORD newVal);

#define GET_CEDRIVES	0xffff
WORD getSetCeDrives(WORD newVal);

#define CALL_OLD_BIOS( function, ... )	\
		useOldBiosHandler = 1;			\
		res = (DWORD) function( __VA_ARGS__ );	\
		useOldBiosHandler = 0;	

#endif

