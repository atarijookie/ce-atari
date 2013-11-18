#define FALSE      0
#define TRUE       1

#define OK         0L           /* OK status */
#define ERROR     -1L           /* ERROR status (timeout) */
#define ERRORL    -2L           /* ERROR status (long timeout) */
#define EWRITF    -10           /* GEMDOS write error code */
#define EREADF    -11           /* GEMDOS read error code */
#define CRITRETRY 0x00010000L   /* RETRY return code */

#define NRETRIES   3            /* number of times to retry -1 */
#define MAX_UNITS  16           /* Max number of drives attached */
#define MAXSECTORS 254          /* Max # sectors for a DMA */
#define MAXRETRIES 1

/* Timing constants */
#define LTIMEOUT   600L         /* long-timeout 3 sec */
#define STIMEOUT    20L         /* short-timeout 100 msec */

/* RWABS flags */
#define RW_FLAG       0x01      /* flag for read/write */
#define MEDIACH_FLAG  0x02      /* flag for read/write with mediachange */
#define RETRY_FLAG    0x04      /* flag for read/write with retries */
#define PHYSOP_FLAG   0x08      /* flag for physical/logical read/write */
/* ------------------------------------------ */

#include <stdint.h>

#define BYTE  	unsigned char
//#define WORD  	unsigned int
//#define DWORD 	unsigned long int
#define WORD  	uint16_t
#define DWORD 	uint32_t


/* ASCI Commands */
#define IO_DINT     0x20        /* DMA interrupt (FDC or HDC) */

#define dmaAddrSectCnt	((WORD *) 0xFF8604)
#define dmaAddrData		((WORD *) 0xFF8604)

#define dmaAddrMode		((WORD *) 0xFF8606)
#define dmaAddrStatus	((WORD *) 0xFF8606)

#define dmaAddrHi		((BYTE *) 0xFF8609)
#define dmaAddrMid		((BYTE *) 0xFF860B)
#define dmaAddrLo		((BYTE *) 0xFF860D)

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

#define FLOCK      ((WORD  *) 0x043E) /* Floppy lock variable */ 
#define HZ_200     ((DWORD *) 0x04BA) /* 200 Hz system clock */ 
/*---------------------------------------*/
BYTE wait_dma_cmpl(DWORD t_ticks);
BYTE fdone(void);
BYTE qdone(void);
void setdma(DWORD addr);
BYTE hdone(void);
BYTE endcmd(WORD mode);

BYTE acsi_cmd(BYTE ReadNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount);
/*---------------------------------------*/

