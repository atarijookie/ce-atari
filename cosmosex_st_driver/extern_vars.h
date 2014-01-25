#ifndef _EXTERN_VARS_H_
#define _EXTERN_VARS_H_

typedef void  (*TrapHandlerPointer)( void );

extern void gemdos_handler( void );
extern TrapHandlerPointer old_gemdos_handler;

extern void bios_handler( void );
extern TrapHandlerPointer old_bios_handler;

/* ------------------------------------------------------------------ */
/* init and hooks part - MiKRO */
extern int16_t useOldGDHandler;											/* 0: use new handlers, 1: use old handlers */
extern int16_t useOldBiosHandler;										/* 0: use new handlers, 1: use old handlers */ 

extern int32_t (*gemdos_table[256])( void* sp );
extern int32_t (  *bios_table[256])( void* sp );

/* ------------------------------------------------------------------ */
/* CosmosEx and Gemdos part - Jookie */

extern BYTE deviceID;
extern BYTE commandShort[CMD_LENGTH_SHORT];
extern BYTE commandLong[CMD_LENGTH_LONG];

extern BYTE *pDta;
extern BYTE tempDta[45];

extern WORD dtaCurrent, dtaTotal;
extern BYTE dtaBuffer[DTA_BUFFER_SIZE + 2];
extern BYTE *pDtaBuffer;
extern BYTE fsnextIsForUs, tryToGetMoreDTAs;

extern WORD ceDrives;
extern WORD ceMediach;
extern BYTE currentDrive;

#endif
