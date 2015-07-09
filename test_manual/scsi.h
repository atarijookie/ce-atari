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

// TT BYTE regs for reading
#define REG_DB      0x1
#define REG_ICR     0x3
#define REG_MR      0x5
#define REG_TCR     0x7
#define REG_CR      0x9
#define REG_DSR     0xb
#define REG_IDR     0xd
#define REG_REI     0xf

// TT BYTE regs for writing
#define REG_ODR     0x1
#define REG_ISR     0x9
#define REG_SDS     0xb
#define REG_DTR     0xd
#define REG_DIR     0xf

#define REG_DMARES  0x10
#define REG_DMACTL  0x11

#define SDMARES		    ((volatile DWORD *)  0xFFFF8710)
#define SDMACTL		    ((volatile WORD *)   0xFFFF8714)	// WORD

// SCSI Interface (NCR 5380) for READ operations
#define SCSIDB	        ((volatile BYTE *)  0xFFFF8781)	// current SCSI data bus
#define SCSIICR	        ((volatile BYTE *)  0xFFFF8783)	// initiator command register
#define SCSIMR	        ((volatile BYTE *)  0xFFFF8785)	// mode register
#define SCSITCR	        ((volatile BYTE *)  0xFFFF8787)	// target command register
#define SCSICR	        ((volatile BYTE *)  0xFFFF8789)	// current SCSI control register
#define SCSIDSR	        ((volatile BYTE *)  0xFFFF878B)	// DMA status register
#define SCSIIDR	        ((volatile BYTE *)  0xFFFF878D)	// input data register
#define SCSIREI	        ((volatile BYTE *)  0xFFFF878F)	// reset error / interrupt

// SCSI Interface (NCR 5380) for WRITE operations
#define SCSIODR	        ((volatile BYTE *)  0xFFFF8781)	// output data register
#define SCSIISR	        ((volatile BYTE *)  0xFFFF8789)	// ID select register
#define SCSIDS	        ((volatile BYTE *)  0xFFFF878B)	// start DMA send
#define SCSIDTR	        ((volatile BYTE *)  0xFFFF878D)	// start DMA target receive
#define SCSIDIR	        ((volatile BYTE *)  0xFFFF878F)	// start DMA initiator receive

// TT SCSI DMA Controller
#define bSDMAPTR_hi     ((volatile BYTE *)  0xFFFF8701)
#define bSDMAPTR_mid_hi	((volatile BYTE *)  0xFFFF8703)
#define bSDMAPTR_mid_lo	((volatile BYTE *)  0xFFFF8705)
#define bSDMAPTR_lo     ((volatile BYTE *)  0xFFFF8707)

#define bSDMACNT_hi	    ((volatile BYTE *)  0xFFFF8709)
#define bSDMACNT_mid_hi	((volatile BYTE *)  0xFFFF870B)
#define bSDMACNT_mid_lo	((volatile BYTE *)  0xFFFF870D)
#define bSDMACNT_lo	    ((volatile BYTE *)  0xFFFF870F)

#define SDMARES		    ((volatile DWORD *)  0xFFFF8710)
#define SDMACTL		    ((volatile WORD *)   0xFFFF8714)	// WORD

#define DMAIN   00
#define DMAOUT  01
#define DMAENA  02
#define DMADIS  00

#define MFP2            ((volatile BYTE *)  0xFFFFFA81)

#define scltmout    201     // SCSI long-timeout  (>1000 ms)

// Falcon DMA controller
#define falconDmaAddrHi     ((volatile BYTE *)  0xFFFF8609)
#define falconDmaAddrMid    ((volatile BYTE *)  0xFFFF860B)
#define falconDmaAddrLo     ((volatile BYTE *)  0xFFFF860D)

#define WDC                 ((volatile WORD *)  0xFFFF8604)
#define WDL                 ((volatile WORD *)  0xFFFF8606)
#define WDSR                ((volatile WORD *)  0xFFFF860F)       // Select Register

// Falcon regs for READing
#define SPCSD           0x88    // SPCSD      - R  Current SCSI Data           
#define SPICR           0x89    // SPICR      - RW Initiator Command Register  
#define SPMR2           0x8A    // SPMR2      - RW Mode Register 2             
#define SPTCR           0x8B    // SPTCR      - RW Target Command Register     
#define SPCSB           0x8C    // SPCSB      - R  Current SCSI Bus Status     
#define SPBSR           0x8D    // SPBSR      - R  Bus and Status              
#define SPIDR           0x8E    // SPIDR      - R  Input Data Register         
#define SPRPI           0x8F    // SPRPI      - R  Reset Parity/Interrupts     

// Falcon regs for writing
#define SPODR           0x88    // SPODR      - W  Output Data Register        
//#define SPICR         0x89    // SPICR      - RW Initiator Command Register  
//#define SPMR2         0x8A    // SPMR2      - RW Mode Register 2             
//#define SPTCR         0x8B    // SPTCR      - RW Target Command Register     
#define SPSER           0x8C    // SPSER      - W  Select Enable Register      
#define SPSDS           0x8D    // SPSDS      - W  Start DMA Send              
#define SPSDT           0x8E    // SPSDT      - W  Start DMA Target Receive    
#define SPSDI           0x8F    // SPSDI      - W  Start DMA Initiator Receive 

#define MR_ARBIT        (1 << 0)
#define MR_DMA          (1 << 1)
#define MR_BSYMON       (1 << 2)
#define MR_EOPEN        (1 << 3)
#define MR_PARIINT      (1 << 4)
#define MR_PARIEN       (1 << 5)
#define MR_TARGMODE     (1 << 6)
#define MR_BLOCKDMA     (1 << 7)

#define BSR_ACK         (1 << 0)
#define BSR_ATN         (1 << 1)
#define BSR_BUSYERR     (1 << 2)
#define BSR_PHASEMATCH  (1 << 3)
#define BSR_IRQACTIVE   (1 << 4)
#define BSR_PARIERR     (1 << 5)
#define BSR_DMAREQ      (1 << 6)
#define BSR_ENDDMA      (1 << 7)

#define ICR_DBUS        (1 << 0)
#define ICR_ATN         (1 << 1)
#define ICR_SEL         (1 << 2)
#define ICR_BSY         (1 << 3)
#define ICR_ACK         (1 << 4)
#define ICR_LA          (1 << 5)
#define ICR_AIP         (1 << 6)
#define ICR_RST         (1 << 7)

#define SCSI_PHASE_MSG_IN       0
#define SCSI_PHASE_MSG_OUT      1
#define SCSI_PHASE_STATUS       4
#define SCSI_PHASE_COMMAND      5
#define SCSI_PHASE_DATA_IN      6
#define SCSI_PHASE_DATA_OUT     7

#define TCR_PHASE_DATA_OUT      0
#define TCR_PHASE_DATA_IN       1
#define TCR_PHASE_CMD           2
#define TCR_PHASE_STATUS        3
#define TCR_PHASE_MESSAGE_OUT   6
#define TCR_PHASE_MESSAGE_IN    7

#endif

