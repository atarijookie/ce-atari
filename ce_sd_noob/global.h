#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#define Clear_home()    (void) Cconws("\33E")
#define Cursor_on()     (void) Cconws("\33e")

#define ACSI_READ           1
#define ACSI_WRITE          0

#define TEST_GET_ACSI_IDS   0x92
#define SCSI_CMD_INQUIRY    0x12

#define SCSI_STATUS_OK      0

#define CMD_LENGTH_SHORT    6
#define CMD_LENGTH_LONG     13

#include <stdint.h>

    #ifndef BYTE
        #define BYTE    unsigned char
        #define WORD    uint16_t
        #define DWORD   uint32_t
    #endif

    #ifndef FALSE
        #define FALSE       0
        #define TRUE        1
    #endif

#define HZ_200     ((volatile DWORD *) 0x04BA) /* 200 Hz system clock */

#endif
