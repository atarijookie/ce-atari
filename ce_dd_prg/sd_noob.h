#ifndef __SD_NOOB_H__
#define __SD_NOOB_H__

#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <unistd.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

// ------------------------------------------
// some SCSI and CE commands
#define SCSI_C_WRITE6           0x0a
#define SCSI_C_READ6            0x08

#define TEST_GET_ACSI_IDS       0x92
#define TEST_SET_ACSI_ID        0x93

// ------------------------------------------
// DEVTYPE values really sent from CE
#define DEVTYPE_OFF             0
#define DEVTYPE_SD              1
#define DEVTYPE_RAW             2
#define DEVTYPE_TRANSLATED      3

extern BYTE FastRAMBuffer[];

//--------------------------------------------------
typedef struct {
    BYTE  id;                       // assigned ACSI ID
    BYTE  isInit;                   // contains if the SD card is present and initialized
    DWORD SCapacity;                // capacity of the card in sectors
} TSDcard;

typedef struct {
    DWORD sectorStart;
    DWORD sectorCount;

    DWORD physicalPerAtariSector;   // how many physical sectors fit into single Atari sector (8, 16, 32)

    BYTE  enabled;
    BYTE  driveNo;                  // specifies the mounted device (A: = 0, B: = 1)
} TSDnoobPartition;

extern TSDcard          SDcard;
extern TSDnoobPartition SDnoobPartition;
extern _BPB             SDbpb;

//--------------------------------------------------
BYTE  gotSDnoobCard(void);
DWORD SDnoobRwabs(WORD mode, BYTE *pBuffer, WORD logicalSectorCount, WORD logicalStartingSector, WORD device);
//--------------------------------------------------

#endif

