// based on AHDI 6.061 sources

#ifndef _SCSI_H_
#define _SCSI_H_

#include "global.h"

// GPIP2 BIT ASSIGNMENTS
#define GPIP2SCSI       (1 << 7)    // SCSI xIRQ
#define GPIP2RTC        (1 << 6)    // RTC IRQ
#define GPIP25          (1 << 5)    // 
#define GPIP2CHGL       (1 << 4)    // ChangeLine
#define GPIP2RI         (1 << 3)    // Ring Indicator (SCC Port B)
#define GPIP2DBE        (1 << 2)    // DMA Bus Error
#define LED1            (1 << 1)    // debug LED
#define LED0            (1 << 0)    // debug LED

#define ICR_REQ         (1 << 5)
#define ICR_BUSY        (1 << 6)

// SCSI Interface (NCR 5380) for READ operations
#define SCSIDB	        ((volatile BYTE *) 0xFFFF8781)	// current SCSI data bus
#define SCSIICR	        ((volatile BYTE *) 0xFFFF8783)	// initiator command register
#define SCSIMR	        ((volatile BYTE *) 0xFFFF8785)	// mode register
#define SCSITCR	        ((volatile BYTE *) 0xFFFF8787)	// target command register
#define SCSICR	        ((volatile BYTE *) 0xFFFF8789)	// current SCSI control register
#define SCSIDSR	        ((volatile BYTE *) 0xFFFF878B)	// DMA status register
#define SCSIIDR	        ((volatile BYTE *) 0xFFFF878D)	// input data register
#define SCSIREI	        ((volatile BYTE *) 0xFFFF878F)	// reset error / interrupt

// SCSI Interface (NCR 5380) for WRITE operations
#define SCSIODR	        ((volatile BYTE *) 0xFFFF8781)	// output data register
#define SCSIISR	        ((volatile BYTE *) 0xFFFF8789)	// ID select register
#define SCSIDS	        ((volatile BYTE *) 0xFFFF878B)	// start DMA send
#define SCSIDTR	        ((volatile BYTE *) 0xFFFF878D)	// start DMA target receive
#define SCSIDIR	        ((volatile BYTE *) 0xFFFF878F)	// start DMA initiator receive

// SCSI DMA Controller
#define bSDMAPTR	    ((volatile BYTE *) 0xFFFF8701)

#define bSDMAPTR_hi     ((volatile BYTE *) 0xFFFF8701)
#define bSDMAPTR_mid_hi	((volatile BYTE *) 0xFFFF8703)
#define bSDMAPTR_mid_lo	((volatile BYTE *) 0xFFFF8705)
#define bSDMAPTR_lo     ((volatile BYTE *) 0xFFFF8707)

#define bSDMACNT	    ((volatile BYTE *) 0xFFFF8709)

#define bSDMACNT_hi	    ((volatile BYTE *) 0xFFFF8709)
#define bSDMACNT_mid_hi	((volatile BYTE *) 0xFFFF870B)
#define bSDMACNT_mid_lo	((volatile BYTE *) 0xFFFF870D)
#define bSDMACNT_lo	    ((volatile BYTE *) 0xFFFF870F)

#define SDMARES		    ((volatile DWORD *) 0xFFFF8710)
#define SDMACTL		    ((volatile WORD *)  0xFFFF8714)	// WORD

#define DMAIN   00
#define DMAOUT  01
#define DMAENA  02
#define DMADIS  00

#define MFP2            ((volatile BYTE *) 0xFFFFFA81)

#define scxltmout   12001   // SCSI extra long-timeout (>1 min)
#define slwsclto    5000    // SCSI long-timeout (>25 S) for stunit()
#define slwscsto    42      // SCSI short-timeout (>205 mS) for stunit()
#define scltmout    201     // SCSI long-timeout (>1000 ms)
#define scstmout    51      // SCSI short-timeout (>500 ms)
#define rcaltm      801     // time for drive recalibration (>4s)

BYTE scsi_cmd(BYTE readNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount);

#endif

