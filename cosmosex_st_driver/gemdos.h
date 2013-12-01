#ifndef GEMDOS_H
#define GEMDOS_H

// path functions
#define GEMDOS_Dsetdrv      0x0e
#define GEMDOS_Dgetdrv      0x19
#define GEMDOS_Dsetpath     0x3b
#define GEMDOS_Dgetpath     0x47

// directory & file search
#define GEMDOS_Fsetdta      0x1a
#define GEMDOS_Fgetdta      0x2f
#define GEMDOS_Fsfirst      0x4e
#define GEMDOS_Fsnext       0x4f

// file and directory manipulation
#define GEMDOS_Dfree        0x36
#define GEMDOS_Dcreate      0x39
#define GEMDOS_Ddelete      0x3a
#define GEMDOS_Frename      0x56
#define GEMDOS_Fdatime      0x57
#define GEMDOS_Fdelete      0x41
#define GEMDOS_Fattrib      0x43

// file content functions
#define GEMDOS_Fcreate      0x3c
#define GEMDOS_Fopen        0x3d
#define GEMDOS_Fclose       0x3e
#define GEMDOS_Fread        0x3f
#define GEMDOS_Fwrite       0x40
#define GEMDOS_Fseek        0x42

// date and time function
#define GEMDOS_Tgetdate     0x2A
#define GEMDOS_Tsetdate     0x2B
#define GEMDOS_Tgettime     0x2C
#define GEMDOS_Tsettime     0x2D

// custom functions - not GEMDOS functions
#define GD_CUSTOM_initialize    0x60
#define GD_CUSTOM_getConfig     0x61
#define GD_CUSTOM_ftell         0x62

// BIOS functions we need to support
#define BIOS_Drvmap				0x70
#define BIOS_Mediach			0x71
#define BIOS_Getbpb				0x72

//////////////////////////////////////
/*

Functions which require more than SCSI(6) length so they wouldn't have to be done in 2 acsi_cmd calls:
GEMDOS_Fseek:  5 + 6
GEMDOS_Fread:  6 + 4
GEMDOS_Fwrite: 6 + 4

Functions which can be implemented with SCSI(6) but would be nicer to do them with something more:
GEMDOS_Fdatime: 6 + 4    -- would be ACSI_READ only then

*/
//////////////////////////////////////

#define MAX_FILES       	40          // maximum open files count, 40 is the value from EmuTOS

#define DTA_BUFFER_SIZE		512			// size of buffer we use to store buffered DTAs on Atari

// file attributes
/*
#define FA_READONLY     (1 << 0)
#define FA_HIDDEN       (1 << 1)
#define FA_SYSTEM       (1 << 2)
#define FA_VOLUME       (1 << 3)
#define FA_DIR          (1 << 4)
#define FA_ARCHIVE      (1 << 5)
*/

// struct used for Fsfirst and Fsnext - modified version without first 21 reserved bytes
// now the struct has 23 bytes total, so a buffer of 512 bytes should contain 22 of these + 6 spare bytes
typedef struct
{
    BYTE    d_attrib;       // GEMDOS File Attributes
    WORD    d_time;         // GEMDOS Time
    WORD    d_date;         // GEMDOS Date
    DWORD   d_length;       // File Length
    char    d_fname[14];    // Filename
} DTAshort;


// list of standard handles used with fdup
#define GSH_CONIN   0   // con: Standard input (defaults to whichever BIOS device is mapped to GEMDOS handle -1)
#define GSH_CONOUT  1   // con: Standard output (defaults to whichever BIOS device is mapped to GEMDOS handle -1)
#define GSH_AUX     2   // aux: Currently mapped serial device (defaults to whichever BIOS device is mapped to GEMDOS handle -2)
#define GSH_PRN     3   // prn: Printer port (defaults to whichever BIOS device is currently mapped to GEMDOS handle -3).
#define GSH_RES1    4   // None Reserved
#define GSH_RES2    5   // None Reserved
#define GSH_BIOSCON -1  // None Refers to BIOS handle 2. This handle may only be redirected under the presence of MiNT. Doing so redirects output of the BIOS.
#define GSH_BIOSAUX -2  // None Refers to BIOS handle 1. This handle may only be redirected under the presence of MiNT. Doing so redirects output of the BIOS.
#define GSH_BIOSPRN -3  // None Refers to BIOS handle 0. This handle may only be redirected under the presence of MiNT. Doing so redirects output of the BIOS.
#define GSH_MIDIIN  -4
#define GSH_MIDIOUT -5

void initFunctionTable(void);

WORD getDriveFromPath(char *path);
BYTE isOurDrive(WORD drive, BYTE withCurrentDrive);
void updateCeDrives(void);

int32_t custom_fread ( void *sp );
int32_t custom_fwrite( void *sp );

BYTE commitChanges(WORD ceHandle);
void initFileBuffer(WORD ceHandle);

BYTE writeData(BYTE ceHandle, BYTE *bfr, DWORD cnt);

#define CALL_OLD_GD( function, ... )	\
		useOldGDHandler = 1;			\
		res = function( __VA_ARGS__ );	\
		useOldGDHandler = 0;			\
		return res;

#define CALL_OLD_GD_NORET( function, ... )	\
		useOldGDHandler = 1;			\
		res = function( __VA_ARGS__ );	\
		useOldGDHandler = 0;			

		
		
// The following macros are used to convert atari handle numbers which are WORDs
// to CosmosEx ex handle numbers, which are only BYTEs; and back.
// To mark the difference between normal Atari handle and handle which came 
// from CosmosEx I've added some offset to CosmosEx handles.

// CosmosEx file handle:              0 ...  40
// Atari handle for regular files:    0 ...  90
// Atari handle for CosmosEx files: 150 ... 200
#define handleIsFromCE(X)		(X >= 150 && X <= 200)
#define handleAtariToCE(X)		(X  - 150)
#define handleCEtoAtari(X)		(X  + 150)		
		
#endif // GEMDOS_H
