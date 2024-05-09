#ifndef _HDD_IF_H_
#define _HDD_IF_H_

#include "global.h"

void scsi_cmd_TT           (uint8_t readNotWrite, uint8_t *cmd, uint8_t cmdLength, uint8_t *buffer, uint16_t sectorCount);
void scsi_cmd_Falcon       (uint8_t readNotWrite, uint8_t *cmd, uint8_t cmdLength, uint8_t *buffer, uint16_t sectorCount);

typedef void  (*THddIfCmd) (uint8_t readNotWrite, uint8_t *cmd, uint8_t cmdLength, uint8_t *buffer, uint16_t sectorCount);
typedef void  (*TsetReg)   (int whichReg, uint32_t value);
typedef uint32_t (*TgetReg)   (int whichReg);

typedef uint8_t  (*TdmaDataTx_prepare) (uint8_t readNotWrite, uint8_t *buffer, uint32_t dataByteCount);
typedef uint8_t  (*TdmaDataTx_do)      (uint8_t readNotWrite, uint8_t *buffer, uint32_t dataByteCount);

uint32_t scsi_getReg_TT(int whichReg);
void  scsi_setReg_TT(int whichReg, uint32_t value);

uint32_t scsi_getReg_Falcon(int whichReg);
void  scsi_setReg_Falcon(int whichReg, uint32_t value);

void  scsi_clrBit(int whichReg, uint32_t bitMask);
void  scsi_setBit(int whichReg, uint32_t bitMask);

uint8_t dmaDataTx_prepare_TT       (uint8_t readNotWrite, uint8_t *buffer, uint32_t dataByteCount);
uint8_t dmaDataTx_do_TT            (uint8_t readNotWrite, uint8_t *buffer, uint32_t dataByteCount);

uint8_t dmaDataTx_prepare_Falcon   (uint8_t readNotWrite, uint8_t *buffer, uint32_t dataByteCount);
uint8_t dmaDataTx_do_Falcon        (uint8_t readNotWrite, uint8_t *buffer, uint32_t dataByteCount);

typedef struct {
    THddIfCmd		    cmd;
    THddIfCmd		    cmd_nolock;
    THddIfCmd           cmd_intern;

    uint8_t                success;
    uint8_t                statusByte;
    uint8_t                phaseChanged;

    int                 retriesDoneCount;
    int                 maxRetriesCount;

    uint8_t                forceFlock;

    TsetReg             pSetReg;
    TgetReg             pGetReg;

    TdmaDataTx_prepare  pDmaDataTx_prepare;
    TdmaDataTx_do       pDmaDataTx_do;

    uint8_t                scsiHostId;
} THDif;

extern THDif hdIf;

//--------------------------------
#define IF_NONE         0
#define IF_ACSI         1
#define IF_SCSI_TT      2
#define IF_SCSI_FALCON  3

void hdd_if_select(int ifType);     // call this function with above define as a param to init the pointers depending on that interface
//--------------------------------

void hdIfCmdAsUser(uint8_t readNotWrite, uint8_t *cmd, uint8_t cmdLength, uint8_t *buffer, uint16_t sectorCount);

#endif
