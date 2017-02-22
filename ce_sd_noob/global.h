#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#define Clear_home()    (void) Cconws("\33E")
#define Cursor_on()     (void) Cconws("\33e")

#define ACSI_READ               1
#define ACSI_WRITE              0

#define SCSI_C_WRITE6           0x0a
#define SCSI_C_READ6            0x08

#define TEST_GET_ACSI_IDS       0x92
#define TEST_SET_ACSI_ID        0x93

#define SCSI_CMD_INQUIRY        0x12
#define SCSI_C_REQUEST_SENSE    0x03

#define SCSI_STATUS_OK                          0
#define SCSI_STATUS_CHECK_CONDITION             2

#define SCSI_ASC_MEDIUM_NOT_PRESENT             0x3A
#define SCSI_ASC_NOT_READY_TO_READY_TRANSITION  0x28

#define CMD_LENGTH_SHORT        6
#define CMD_LENGTH_LONG         13

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
