//--------------------------------------------
// Defines for UltraSatan
// Date: 2008-02-24
//--------------------------------------------
#include <cdefBF531.h>
#include "pin_defines.h"

#ifndef __MYDEFINES_H_
#define __MYDEFINES_H_
//--------------------------------------------
#define TRUE	1
#define FALSE	0
/////////////////////////////////////////////////////////
#define VERSION_STRING          "UltraSatan v1.13 (by Jookie)"
#define VERSION_STRING_SHORT    "1.13"
#define DATE_STRING             "05/01/13"                  // MM/DD/YY
/////////////////////////////////////////////////////////
// ACSI signals located at Async memory port
#define pASYNCMEM 	((volatile unsigned short *)0x20000000)

/////////////////////////////////////////////////////////
typedef unsigned char		BYTE;
typedef unsigned short	WORD;
typedef unsigned long		DWORD;
//-------------------------------------------------------------------------------------
// Set or clear flag(s) in a register
#define SET(rgstr, flags)        ((rgstr) = (rgstr) | (flags))
#define CLEAR(rgstr, flags)      ((rgstr) &= ~(flags))

// Poll the status of flags in a register
#define ISSET(rgstr, flags)      (((rgstr) & (flags)) == (flags))
#define ISCLEARED(rgstr, flags)  (((rgstr) & (flags)) == 0)

// Returns the higher/lower byte of a word
#define HBYTE(word)                 ((BYTE) ((word) >> 8))
#define LBYTE(word)                 ((BYTE) ((word) & 0x00FF))
//-------------------------------------------------------------------------------------
#define	DEVICETYPE_NOTHING		0x00
#define	DEVICETYPE_MMC				0x01
#define	DEVICETYPE_SD					0x02
#define	DEVICETYPE_SDHC				0x03
#define	DEVICETYPE_RTC				0x04

//-------------------------------------------------------------------------------------
#define	MAX_DEVICES		2
//-------------------------------------------------------------------------------------
typedef struct _TDevice
{
	BYTE 	ACSI_ID;				// ID on the ACSI bus - from 0 to 7
	BYTE 	Type;						// DEVICETYPE_...
	BYTE	IsInit;					// is initialized and working? TRUE / FALSE

	BYTE	MediaChanged;		// when media is changed
	
	BYTE	InitRetries;		// how many times try to init the device */
	
	DWORD	BCapacity;			// device capacity in bytes
	DWORD	SCapacity;			// device capacity in sectors
	
	BYTE	LastStatus;			// last returned SCSI status
	BYTE	SCSI_ASC;				// additional sense code
	BYTE	SCSI_ASCQ;			// additional sense code qualifier
	BYTE	SCSI_SK;				// sense key
} TDevice;
//-------------------------------------------------------------------------------------
#endif
