#if !defined(__GEMDOS_ERRNO)
#define __GEMDOS_ERRNO

#define E_NOTHANDLED    0x7f        // return this if the host part didn't handle the command and we should use the original one

// BIOS errors
#define E_OK            0   // No error
#define GENERIC_ERROR   -1  // Generic error
#define EDRVNR          -2  // Drive not ready
#define EUNCMD          -3  // Unknown command
#define E_CRC           -4  // CRC error
#define EBADRQ          -5  // Bad request
#define E_SEEK          -6  // Seek error
#define EMEDIA          -7  // Unknown media
#define ESECNF          -8  // Sector not found
#define EPAPER          -9  // Out of paper
#define EWRITF          -10 // Write fault
#define EREADF          -11 // Read fault
#define EWRPRO          -12 // Device is write protected
#define E_CHNG          -14 // Media change detected
#define EUNDEV          -15 // Unknown device
#define EBADSF          -16 // Bad sectors on format
#define EOTHER          -17 // Insert other disk (request)

// GEMDOS errors
#define EINVFN          -32 // Invalid function
#define EFILNF          -33 // File not found
#define EPTHNF          -34 // Path not found
#define ENHNDL          -35 // No more handles
#define EACCDN          -36 // Access denied
#define EIHNDL          -37 // Invalid handle
#define ENSMEM          -39 // Insufficient memory
#define EIMBA           -40 // Invalid memory block address
#define EDRIVE          -46 // Invalid drive specification
#define ENSAME          -48 // Cross device rename
#define ENMFIL          -49 // No more files
#define ELOCKED         -58 // Record is already locked
#define ENSLOCK         -59 // Invalid lock removal request
#define ERANGEERROR     -64 // Range error
#define ENAME_TOOLONG   -64 // Range error
#define EINTRN          -65 // Internal error
#define EPLFMT          -66 // Invalid program load format
#define EGSBF           -67 // Memory block growth failure
#define ELOOP           -80 // Too many symbolic links

#endif

/************************************************************************/
