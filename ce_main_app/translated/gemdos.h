#ifndef GEMDOS_H
#define GEMDOS_H

// path functions
#define GEMDOS_Dsetdrv      0x0e
/* ACSI/SCSI command arguments :
 * arg1 = drive index
 * returns a buffer with :
 * offset 0  WORD drive bitmap (bitmap of enabled drives from A to P)
 * returns E_OK or E_NOTHANDLED */
#define GEMDOS_Dgetdrv      0x19
/* No argument
 * return index of current drive (0 to 15) */
#define GEMDOS_Dsetpath     0x3b
/* ACSI/SCSI command arguments :
 * data buffer containing the path
 * returns E_OK or E_NOTHANDLED / EINTRN / E_WAITING_FOR_MOUNT / EPTHNF */
#define GEMDOS_Dgetpath     0x47
/* ACSI/SCSI command arguments :
 * arg1 = drive index + 1 (0 is for current drive)
 * returns a buffer with :
 * offset 0  current path for drive
 * returns E_OK or E_NOTHANDLED */

// directory & file search
#define GEMDOS_Fsetdta      0x1a
/* Fsetdta is handled on ST only */
#define GEMDOS_Fgetdta      0x2f
/* Fgetdta is handled on ST only */
#define GEMDOS_Fsfirst      0x4e
/* ACSI/SCSI command arguments :
 * data buffer containing :
 * offset 0  DWORD dta address on ST used as identifier
 * offset 4  BYTE find attributes FA_READONLY, FA_HIDDEN, FA_SYSTEM, FA_DIR, FA_ARCHIVE
 * offset 5  character string of the search string (C:\PATH\*.*)
 * returns E_OK or E_NOTHANDLED / EINTRN / E_WAITING_FOR_MOUNT / EFILNF */
#define GEMDOS_Fsnext       0x4f
/* ACSI/SCSI command arguments :
 * arg1,arg2,arg3,arg4 = DWORD dta address on ST used as identifier
 * arg5,arg6 = WORD index of first item to send to ST
 * returns a buffer with :
 * offset 0   WORD count of DTA transfered
 * offset 2   n * DTA structures, 23bytes each (see DTAshort below)
 * returns E_OK or EINTRN / EIHNDL / ENMFIL */

// file and directory manipulation
#define GEMDOS_Dfree        0x36
/* ACSI/SCSI command arguments :
 * arg1 = drive index
 * returns a buffer with :
 * offset 0   DWORD free cluster count
 * offset 4   DWORD total cluster count
 * offset 8   DWORD sector size (512)
 * offset 12  DWORD sector count per cluster
 * returns E_OK or E_NOTHANDLED */
#define GEMDOS_Dcreate      0x39
/* ACSI/SCSI command arguments :
 * 512 byte data buffer :
 *   null terminated ASCII string
 * returns E_OK or E_NOTHANDLED / EACCDN / EINTRN / EPTHNF / E_WAITING_FOR_MOUNT */
#define GEMDOS_Ddelete      0x3a
/* ACSI/SCSI command arguments :
 * 512 byte data buffer :
 *   null terminated ASCII string
 * returns E_OK or E_NOTHANDLED / EACCDN / EINTRN / EPTHNF / E_WAITING_FOR_MOUNT */
#define GEMDOS_Frename      0x56
/* ACSI/SCSI command arguments :
 * 512 byte data buffer :
 *   2 null terminated ASCII string : old name then new name
 * returns E_OK or E_NOTHANDLED / EACCDN / EFILNF / E_WAITING_FOR_MOUNT */
#define GEMDOS_Fdatime      0x57
/* ACSI/SCSI command arguments :
 * arg1 = file handle (7 lower bits) + "set" bit (0 is get, 1 is set)
 * arg2,arg3 = time
 * arg4,arg5 = date
 * return a data buffer
 *  offset 0 : WORD time
 *  offset 2 : DORD date
 * returns E_OK or E_NOTHANDLED / EINTRN */
#define GEMDOS_Fdelete      0x41
/* ACSI/SCSI command arguments :
 * 512 byte data buffer :
 *   null terminated ASCII string
 * returns E_OK or E_NOTHANDLED / EACCDN / EINTRN / EFILNF / E_WAITING_FOR_MOUNT */
#define GEMDOS_Fattrib      0x43
/* ACSI/SCSI command arguments :
 * 512 byte data buffer :
 *  offset 0 : 0 to inquire, non zero to set
 *  offset 1 : new attribute byte
 *  offset 2 :  null terminated ASCII string
 * returns E_OK or E_NOTHANDLED / EACCDN / EINTRN / EFILNF / E_WAITING_FOR_MOUNT */

// file content functions
#define GEMDOS_Fcreate      0x3c
/* ACSI/SCSI command arguments :
 * 512 byte data buffer :
 * offset 0 : 1 byte = attributes
 * offset 1 : n bytes = file name
 * returns EINTRN / EACCDN / E_NOTHANDLED / ENHNDL / E_WAITING_FOR_MOUNT or handle */
#define GEMDOS_Fopen        0x3d
/* ACSI/SCSI command arguments :
 * 512 byte data buffer :
 * offset 0 : 1 byte = mode (0 = read / 1 = write / 2 = read write)
 * offset 1 : n bytes = file name
 * returns EINTRN / EACCDN / E_NOTHANDLED / ENHNDL / E_WAITING_FOR_MOUNT or handle */
#define GEMDOS_Fclose       0x3e
/* ACSI/SCSI command arguments :
 * arg1 = file handle
 * returns E_NOTHANDLED / EINTRN / E_OK */
#define GEMDOS_Fread        0x3f
/* ACSI/SCSI command arguments :
 * arg1 = file handle
 * arg2,arg3,arg4 = byte count
 * arg5 = seek offset
 * return a data buffer
 * returns E_NOTHANDLED / EINTRN / RW_ALL_TRANSFERED / RW_PARTIAL_TRANSFER */
#define GEMDOS_Fwrite       0x40
/* ACSI/SCSI command arguments :
 * arg1 = file handle
 * arg2,arg3,arg4 = byte count
 * + a data buffer
 * returns E_NOTHANDLED / EINTRN / RW_ALL_TRANSFERED / RW_PARTIAL_TRANSFER */
#define GEMDOS_Fseek        0x42
/* ACSI/SCSI command arguments :
 * arg1,arg2,arg3,arg4 = offset
 * arg5 = handle
 * arg6 = seek mode (0=SEEK_SET/1=SEEK_CUR/2=SEEK_END)
 * returns a data buffer :
 * 4 bytes : current position
 * 4 bytes : bytes count to end of file
 * returns E_NOTHANDLED / EINTRN / E_OK */

// Pexec() related stuff
#define GEMDOS_Pexec        0x4B
/* ACSI/SCSI command arguments :
 * arg1 = subcommand : PEXEC_CREATE_IMAGE / PEXEC_GET_BPB / PEXEC_READ_SECTOR / PEXEC_WRITE_SECTOR
 * - PEXEC_CREATE_IMAGE :
 *   512 byte data buffer :
 *   offset 0  WORD mode (ignored ?)
 *   offset 2  char string of program path
 *   returns E_NOTHANDLED / E_WAITING_FOR_MOUNT / EFILNF / EACCDN / EINTRN / E_OK
 * - PEXEC_GET_BPB :
 *   returns a data buffer :
 *   offset 0   BPB (20 bytes)
 *   offset 32  path to PRG file
 *   offset 256 PRG name without path
 *   offset 384 PRG path
 *   returns E_OK
 * - PEXEC_READ_SECTOR :
 *   arg2,arg3 : WORD starting sector
 *   arg4,arg5 : WORD sector count
 *   returns a byte buffer with the requested sector data
 *   returns E_OK or EINTRN
 * - PEXEC_WRITE_SECTOR :
 *   arg2,arg3 : WORD starting sector
 *   arg4,arg5 : WORD sector count
 *   + a data buffer for sector data
 *   returns E_OK or EINTRN
 */

// date and time function
#define GEMDOS_Tgetdate     0x2A
/* Tgetdate is handled on ST only */
#define GEMDOS_Tsetdate     0x2B
/* Tsetdate is handled on ST only */
#define GEMDOS_Tgettime     0x2C
/* Tgettime is handled on ST only */
#define GEMDOS_Tsettime     0x2D
/* Tsettime is handled on ST only */

#define GD_CUSTOM_initialize    0x60
/* called on the startup of CosmosEx translated disk driver
 * ACSI/SCSI command arguments :
 * 512 byte data buffer :
 * offset 0  WORD TOS Version
 * offset 2  WORD current resolution
 * offset 4  drives bitmap
 * returns E_OK or EINTRN */
#define GD_CUSTOM_getConfig     0x61
/* no argument
 * returns a buffer :
 * offset 0  WORD drive bitmap
 * offset 2  BYTE first translated drive
 * offset 3  BYTE shared drive
 * offset 4  BYTE config drive
 * offset 5  BYTE set date time flag
 * offset 6  BYTE UTC offset (in hours * 10)
 * offset 7  WORD (unaligned !) year
 * offset 9  BYTE month
 * offset 10 BYTE day
 * offset 11 BYTE hours
 * offset 12 BYTE minutes
 * offset 13 BYTE seconds
 * offset 14 BYTE eth0 enabled flag
 * offset 15 DWORD (unaligned) eth0 IP address
 * offset 19 BYTE wlan0 enabled flag
 * offset 20 DWORD wlan0 IP address
 * offset 24 BYTE frame skip for screencast
 * offset 25 WORD (unaligned) TRANSLATEDDISK_VERSION
 * offset 27 BYTE screen shot vbl enabled
 * offset 28 BYTE take screenshot ?
 * returns E_OK */
#define GD_CUSTOM_ftell         0x62
/* ACSI/SCSI command arguments :
 * arg1 = file handle
 * returns a data buffer :
 * offset 0 DWORD position
 * returns E_NOTHANDLED / EINTRN / E_OK */
#define GD_CUSTOM_getRWdataCnt  0x63
/* ACSI/SCSI command arguments :
 * arg1 = file handle
 * returns a data buffer :
 * offset 0 DWORD last data count
 * returns E_NOTHANDLED / EINTRN / E_OK */
#define GD_CUSTOM_Fsnext_last   0x64
/* ACSI/SCSI command arguments :
 * arg1,arg2,arg3,arg4 = DWORD dta address on ST used as identifier
 * clear the structures used by Fsfirst/Fsnext
 * returns E_OK or EIHNDL */
#define GD_CUSTOM_getBytesToEOF 0x65
/* ACSI/SCSI command arguments :
 * arg1 = file handle
 * returns a data buffer :
 * offset 0 DWORD byte count to end of file
 * returns E_NOTHANDLED / EINTRN / E_OK */

// BIOS functions we need to support
#define BIOS_Drvmap				0x70
#define BIOS_Mediach			0x71
#define BIOS_Getbpb				0x72

// other functions
#define ACC_GET_MOUNTS			0x80
#define ACC_UNMOUNT_DRIVE       0x81

#define ST_LOG_TEXT             0x85

#define TEST_READ               0x90
#define TEST_WRITE              0x91
#define TEST_GET_ACSI_IDS       0x92

//////////////////////////////////////

// file attributes
#define FA_READONLY     (1 << 0)
#define FA_HIDDEN       (1 << 1)
#define FA_SYSTEM       (1 << 2)
#define FA_VOLUME       (1 << 3)
#define FA_DIR          (1 << 4)
#define FA_ARCHIVE      (1 << 5)

#define FA_ALL          (0x3f)

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

//////////////////////////////////////

#define PEXEC_CREATE_IMAGE      0
#define PEXEC_GET_BPB           1
#define PEXEC_READ_SECTOR       2
#define PEXEC_WRITE_SECTOR      3

//////////////////////////////////////

#endif // GEMDOS_H
