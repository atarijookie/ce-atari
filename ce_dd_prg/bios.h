#ifndef _BIOS_H_
#define _BIOS_H_

int32_t custom_mediach( void *sp );
int32_t custom_drvmap ( void *sp );
int32_t custom_getbpb ( void *sp );

void updateCeDrives(void);
void updateCeMediach(void);

#define CALL_OLD_BIOS( function, ... )	\
		useOldBiosHandler = 1;			\
		res = (DWORD) function( __VA_ARGS__ );

#endif
