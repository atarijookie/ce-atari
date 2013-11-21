#if !defined(__GEMDOS_ERRNO)
#define __GEMDOS_ERRNO

#define E_NOTHANDLED    0x7f        // return this if the host part didn't handle the command and we should use the original one

// BIOS errors
#define E_OK            0   	// 00 No error
#define GENERIC_ERROR   0xff	// -1  // ff Generic error
#define EDRVNR          0xfe	// -2  // fe Drive not ready
#define EUNCMD          0xfd	// -3  // fd Unknown command
#define E_CRC           0xfc	// -4  // fc CRC error
#define EBADRQ          0xfb	// -5  // fb Bad request
#define E_SEEK          0xfa	// -6  // fa Seek error
#define EMEDIA          0xf9	// -7  // f9 Unknown media
#define ESECNF          0xf8	// -8  // f8 Sector not found
#define EPAPER          0xf7	// -9  // f7 Out of paper
#define EWRITF          0xf6	// -10 // f6 Write fault
#define EREADF          0xf5	// -11 // f5 Read fault
#define EWRPRO          0xf4	// -12 // f4 Device is write protected
#define E_CHNG          0xf2	// -14 // f2 Media change detected
#define EUNDEV          0xf1	// -15 // f1 Unknown device
#define EBADSF          0xf0	// -16 // f0 Bad sectors on format
#define EOTHER          0xef	// -17 // ef Insert other disk (request)

// GEMDOS errors
#define EINVFN          0xe0	// -32 // e0 Invalid function
#define EFILNF          0xdf	// -33 // df File not found
#define EPTHNF          0xde	// -34 // de Path not found
#define ENHNDL          0xdd	// -35 // dd No more handles
#define EACCDN          0xdc	// -36 // dc Access denied
#define EIHNDL          0xdb	// -37 // db Invalid handle
#define ENSMEM          0xd9	// -39 // d9 Insufficient memory
#define EIMBA           0xd8	// -40 // d8 Invalid memory block address
#define EDRIVE          0xd2	// -46 // d2 Invalid drive specification
#define ENSAME          0xd0	// -48 // d0 Cross device rename
#define ENMFIL          0xcf	// -49 // cf No more files
#define ELOCKED         0xc6	// -58 // c6 Record is already locked
#define ENSLOCK         0xc5	// -59 // c5 Invalid lock removal request
#define ERANGEERROR     0xc0	// -64 // c0 Range error
#define ENAME_TOOLONG   0xc0	// -64 // c0 Range error
#define EINTRN          0xbf	// -65 // bf Internal error
#define EPLFMT          0xbe	// -66 // be Invalid program load format
#define EGSBF           0xbd	// -67 // bd Memory block growth failure
#define ELOOP           0xb0	// -80 // b0 Too many symbolic links

#endif

/************************************************************************/
