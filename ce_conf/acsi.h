#ifndef _ACSI_H_
#define _ACSI_H_

/* ------------------------------------------ */

#define OK			0           /* OK status */
#define ACSIERROR	0xff        /* ERROR status (timeout) */

#define MAXSECTORS	254          /* Max # sectors for a DMA */

/* Timing constants */
#define LTIMEOUT	600L         /* long-timeout 3 sec */
#define STIMEOUT	20L         /* short-timeout 100 msec */

/* ------------------------------------------ */

#include <stdint.h>

#ifndef BYTE
    #define BYTE  	unsigned char
    #define WORD  	uint16_t
    #define DWORD 	uint32_t
#endif

/* mfp chip register */ 
#define mfpGpip			((volatile BYTE *) 0xFFFA01)

/* DMA chip registers and flag */
#define IO_DINT     0x20        /* DMA interrupt (FDC or HDC) */

#define dmaAddrSectCnt	((volatile WORD *) 0xFF8604)
#define dmaAddrData		((volatile WORD *) 0xFF8604)

#define dmaAddrMode		((volatile WORD *) 0xFF8606)
#define dmaAddrStatus	((volatile WORD *) 0xFF8606)

#define dmaAddrHi		((volatile BYTE *) 0xFF8609)
#define dmaAddrMid		((volatile BYTE *) 0xFF860B)
#define dmaAddrLo		((volatile BYTE *) 0xFF860D)

/*---------------------------------------*/

/* Mode Register bits */
#define NOT_USED     0x0001     /* not used bit */
#define A0           0x0002     /* A0 line, A1 on DMA port */
#define A1           0x0004     /* A1 line, not used on DMA port */
#define HDC          0x0008     /* HDC / FDC register select */
#define SC_REG       0x0010     /* Sector count register select */
#define RESERVED5    0x0020     /* reserved for future expansion ? */
#define RESERVED6    0x0040     /* bit has no function */
#define NO_DMA       0x0080     /* disable / enable DMA transfer */
#define DMA_WR       0x0100     /* Write to / Read from DMA port */

/* Status Register bits */
#define DMA_OK       0x0001     /* DMA transfer went OK */
#define SC_NOT_0     0x0002     /* Sector count register not zero */
#define DATA_REQ     0x0004     /* DRQ line state */

#define FLOCK      ((volatile WORD  *) 0x043E) /* Floppy lock variable */ 
#define HZ_200     ((volatile DWORD *) 0x04BA) /* 200 Hz system clock */ 

#define ACSI_READ	1
#define ACSI_WRITE	0

#define SCSI_CMD_INQUIRY	0x12

#define CMD_LENGTH_SHORT	6
#define CMD_LENGTH_LONG		13
/*---------------------------------------*/
BYTE wait_dma_cmpl(DWORD t_ticks);
BYTE fdone(void);
BYTE qdone(void);
void setdma(DWORD addr);
BYTE hdone(void);
BYTE endcmd(WORD mode);

BYTE acsi_cmd(BYTE ReadNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount);
/*---------------------------------------*/

#endif
