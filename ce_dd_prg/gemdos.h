#ifndef GEMDOS_H
#define GEMDOS_H

// path functions
#define GEMDOS_Dsetdrv      	0x0e
#define GEMDOS_Dgetdrv      	0x19
#define GEMDOS_Dsetpath     	0x3b
#define GEMDOS_Dgetpath     	0x47

// directory & file search
#define GEMDOS_Fsetdta      	0x1a
#define GEMDOS_Fgetdta      	0x2f
#define GEMDOS_Fsfirst      	0x4e
#define GEMDOS_Fsnext       	0x4f

// file and directory manipulation
#define GEMDOS_Dfree        	0x36
#define GEMDOS_Dcreate      	0x39
#define GEMDOS_Ddelete      	0x3a
#define GEMDOS_Frename      	0x56
#define GEMDOS_Fdatime      	0x57
#define GEMDOS_Fdelete      	0x41
#define GEMDOS_Fattrib      	0x43

// file content functions
#define GEMDOS_Fcreate      	0x3c
#define GEMDOS_Fopen        	0x3d
#define GEMDOS_Fclose       	0x3e
#define GEMDOS_Fread        	0x3f
#define GEMDOS_Fwrite       	0x40
#define GEMDOS_Fseek        	0x42

// date and time function
#define GEMDOS_Tgetdate     	0x2A
#define GEMDOS_Tsetdate     	0x2B
#define GEMDOS_Tgettime     	0x2C
#define GEMDOS_Tsettime     	0x2D

// program execution functions
#define GEMDOS_pexec            0x4b
#define GEMDOS_pterm            0x4c
#define GEMDOS_pterm0           0x00
#define GEMDOS_ptermres         0x31

// custom functions - not GEMDOS functions
#define GD_CUSTOM_initialize    0x60
#define GD_CUSTOM_getConfig     0x61
#define GD_CUSTOM_ftell         0x62
#define GD_CUSTOM_getRWdataCnt  0x63
#define GD_CUSTOM_Fsnext_last   0x64
#define GD_CUSTOM_getBytesToEOF 0x65

// BIOS functions we need to support
#define BIOS_Drvmap				0x70
#define BIOS_Mediach			0x71
#define BIOS_Getbpb				0x72

#define ST_LOG_TEXT             0x85
//////////////////////////////////////
/*

Functions which require more than SCSI(6) length so they wouldn't have to be done in 2 hdd_if_cmd calls:
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
    uint8_t    d_attrib;       // GEMDOS File Attributes
    uint16_t    d_time;         // GEMDOS Time
    uint16_t    d_date;         // GEMDOS Date
    uint32_t   d_length;       // File Length
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

uint16_t getDriveFromPath(const char *path);
uint8_t isOurDrive(uint16_t drive, uint8_t withCurrentDrive);

int32_t custom_fread ( void *sp );
int32_t custom_fwrite( void *sp );
int32_t custom_pexec( void *sp );
int32_t custom_pterm( void *sp );
int32_t custom_pterm0( void *sp );

uint8_t commitChanges(uint16_t ceHandle);
void initFileBuffer(uint16_t ceHandle);

uint8_t fillReadBuffer(uint16_t ceHandle);
uint32_t readData(uint16_t ceHandle, uint8_t *bfr, uint32_t cnt, uint8_t seekOffset);
void seekInFileBuffer(uint16_t ceHandle, int32_t offset, uint8_t seekMode);
void getBytesToEof(uint16_t ceHandle);

uint32_t writeData(uint8_t ceHandle, uint8_t *bfr, uint32_t cnt);

#define CALL_OLD_GD( function, ... )	\
		useOldGDHandler = 1;			\
		res = function( __VA_ARGS__ );	\
		return res;

#define CALL_OLD_GD_NORET( function, ... )	\
		useOldGDHandler = 1;			\
		res = function( __VA_ARGS__ );

#define CALL_OLD_GD_VOIDRET( function, ... )	\
		useOldGDHandler = 1;			\
		function( __VA_ARGS__ );

// if sign bit is set, extend the sign to whole uint32_t
//#define extendByteToDword(X)    ( ((X & 0x80)==0) ? X : (0xffffff00 | X) )
#define extendByteToDword(B)    ((int32_t)((int8_t)(B)))

// macros for getting data from buffer - defined for big and little endian
// big endian (atari)
#define getWord(POINTER)        ((uint16_t)  *((uint16_t  *) (POINTER)))
#define getDword(POINTER)       ((uint32_t) *((uint32_t *) (POINTER)))

// little endian (pc)
//#define getWord(POINTER)        ( (((uint16_t) *POINTER)<<8) | ((uint16_t) *(POINTER+1)))
//#define getDword(POINTER)       ( (((uint32_t) *POINTER)<<24) | (((uint32_t) *(POINTER+1))<<16) | (((uint32_t) *(POINTER+2))<<8) | (((uint32_t) *(POINTER+3))) )

#define GET_WORD(PTR)           (((uint16_t) (PTR)[0]) << 8) | ((uint16_t) (PTR)[1])
#define SET_WORD(PTR,VALUE)     (PTR)[0] = (uint8_t) (VALUE >>  8); (PTR)[1] = (uint8_t) (VALUE      );
#define SET_DWORD(PTR,VALUE)    (PTR)[0] = (uint8_t) (VALUE >> 24); (PTR)[1] = (uint8_t) (VALUE >> 16); (PTR)[2] = (uint8_t) (VALUE >> 8); (PTR)[3] = (uint8_t) VALUE;

// The following macros are used to convert atari handle numbers which are WORDs
// to CosmosEx ex handle numbers, which are only BYTEs; and back.
// To mark the difference between normal Atari handle and handle which came
// from CosmosEx I've added some offset to CosmosEx handles.

// CosmosEx file handle (internally): 0 ...  40
// Atari handle for regular files:    0 ...  79
// Atari handle for CosmosEx files:  80 ... 120
#define handleIsFromCE(X)		(X >= 80 && X <= 120)
#define handleAtariToCE(X)		(X  - 80)
#define handleCEtoAtari(X)		(X  + 80)

//----------------------
// the following buffer type is used for file reading and writing
#define RW_BUFFER_SIZE		512

typedef struct
{
    uint8_t isOpen;                    // if non-zero, the file is open

	uint16_t rCount;					// how much data is buffer (specifies where the next read data could go)
	uint16_t rStart;					// starting index of where we should start reading the buffer
	uint8_t rBuf[RW_BUFFER_SIZE];

    uint32_t currentPos;               // current position in the stream, from the start of the file
    uint32_t bytesToEOF;               // count of bytes until the EOF
    uint8_t  bytesToEOFinvalid;        // flag that marks that before any READ operation you should

	uint16_t wCount;					// how much data we have in this buffer
	uint8_t wBuf[RW_BUFFER_SIZE];

} TFileBuffer;

#endif // GEMDOS_H
