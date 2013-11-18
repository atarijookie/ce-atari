/* ------------------------------------------ */

#define FALSE      0
#define TRUE       1

#define OK         0L           /* OK status */
#define ERROR     -1L           /* ERROR status (timeout) */
#define ERRORL    -2L           /* ERROR status (long timeout) */

#define MAXSECTORS 254          /* Max # sectors for a DMA */

/* Timing constants */
#define LTIMEOUT   600L         /* long-timeout 3 sec */
#define STIMEOUT    20L         /* short-timeout 100 msec */

/* ------------------------------------------ */

#include <stdint.h>

#define BYTE  	unsigned char
#define WORD  	uint16_t
#define DWORD 	uint32_t

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
/*---------------------------------------*/
BYTE wait_dma_cmpl(DWORD t_ticks);
BYTE fdone(void);
BYTE qdone(void);
void setdma(DWORD addr);
BYTE hdone(void);
BYTE endcmd(WORD mode);

BYTE acsi_cmd(BYTE ReadNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount);
/*---------------------------------------*/

