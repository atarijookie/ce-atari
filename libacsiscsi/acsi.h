#ifndef _ACSI_H_
#define _ACSI_H_

#include "global.h"

// ------------------------------------------

#define OK			0           // OK status
#define ACSIERROR	0xff        // ERROR status (timeout)

#define MAXSECTORS	254         // Max # sectors for a DMA

// Timing constants
#define ACSI_TIMEOUT_LONG	600L        // long-timeout 3 sec
#define ACSI_TIMEOUT_SHORT	20L         // short-timeout 100 msec

// ------------------------------------------

// mfp chip register
#define mfpGpip			((volatile uint8_t *) 0xFFFA01)

// DMA chip registers and flag
#define IO_DINT     0x20        // DMA interrupt (FDC or HDC)

#define dmaAddrSectCnt	((volatile uint16_t *) 0xFF8604)
#define dmaAddrData		((volatile uint16_t *) 0xFF8604)

#define dmaAddrMode		((volatile uint16_t *) 0xFF8606)
#define dmaAddrStatus	((volatile uint16_t *) 0xFF8606)

#define dmaAddrHi		((volatile uint8_t *) 0xFF8609)
#define dmaAddrMid		((volatile uint8_t *) 0xFF860B)
#define dmaAddrLo		((volatile uint8_t *) 0xFF860D)

//---------------------------------------

// Mode Register bits
#define NOT_USED     0x0001     // not used bit
#define A0           0x0002     // A0 line, A1 on DMA port
#define A1           0x0004     // A1 line, not used on DMA port
#define HDC          0x0008     // HDC / FDC register select
#define SC_REG       0x0010     // Sector count register select
#define RESERVED5    0x0020     // reserved for future expansion ?
#define RESERVED6    0x0040     // bit has no function
#define NO_DMA       0x0080     // disable / enable DMA transfer
#define DMA_WR       0x0100     // Write to / Read from DMA port

// Status Register bits
#define DMA_OK       0x0001     // DMA transfer went OK
#define SC_NOT_0     0x0002     // Sector count register not zero
#define DATA_REQ     0x0004     // DRQ line state

#define FLOCK      ((volatile uint16_t  *) 0x043E) // Floppy lock variable

#define ACSI_READ	1
#define ACSI_WRITE	0

#define SCSI_CMD_INQUIRY	    0x12
#define SCSI_CMD_READ6          0x08
#define SCSI_CMD_REQUEST_SENSE  0x03
#define SCSI_CMD_WRITE6         0x0a

#define CMD_LENGTH_SHORT	6
#define CMD_LENGTH_LONG		13
//---------------------------------------
uint8_t wait_dma_cmpl(uint32_t t_ticks);
uint8_t fdone(void);
uint8_t qdone(void);
void setdma(uint32_t addr);
uint8_t hdone(void);
void endcmd(uint16_t mode);

void acsi_cmd(uint8_t ReadNotWrite, uint8_t *cmd, uint8_t cmdLength, uint8_t *buffer, uint16_t sectorCount);
//---------------------------------------
#ifdef ONPC

typedef struct
{
    uint8_t ReadNotWrite;
    uint8_t cmd[14];
    uint8_t cmdLength;
    uint8_t sCountHi, sCountLo;
} __attribute__((packed)) AcsiStream;
//---------------------------------------

#endif

#endif
