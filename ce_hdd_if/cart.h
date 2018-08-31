#ifndef _CART_H_
#define _CART_H_

#include "global.h"

#define CART_BASE             0xFB0000
#define CC_CART_OFFSET        0x00C000
#define ofs_cmd_status          0x1200
#define ofs_cmd_pio_1st_byte    0x0c00
#define ofs_cmd_pio_write       0x0400
#define ofs_cmd_pio_read        0x0600
#define ofs_cmd_dma_write       0x0000
#define ofs_cmd_dma_read        0x0200

#define pCmdStatus  ((volatile WORD *)(CART_BASE + CC_CART_OFFSET + ofs_cmd_status))
#define pPIOfirst   ((volatile WORD *)(CART_BASE + CC_CART_OFFSET + ofs_cmd_pio_1st_byte))
#define pPIOwrite   ((volatile WORD *)(CART_BASE + CC_CART_OFFSET + ofs_cmd_pio_write))
#define pPIOread    ((volatile WORD *)(CART_BASE + CC_CART_OFFSET + ofs_cmd_pio_read))
#define pDMAwrite   ((volatile WORD *)(CART_BASE + CC_CART_OFFSET + ofs_cmd_dma_write))
#define pDMAread    ((volatile WORD *)(CART_BASE + CC_CART_OFFSET + ofs_cmd_dma_read))

#define STATUS_CMD      (1 << 2)    // 1st cmd byte was just sent by ST, cleared by RPi
#define STATUS_DRQ      (1 << 1)    // RPi wants to send/receive in DMA mode -- set by RPi, cleared by ST
#define STATUS_INT      (1 << 0)    // RPi wants to send/receive in PIO mode -- set by RPi, cleared by ST
#define STATUS_INT_DRQ  (STATUS_DRQ | STATUS_INT)

#define STATUS2_DataIsPIOread   (1 << 17)   // when H, the read byte was the last byte - ST status byte (transfered using PIO read)
#define STATUS2_DataChanged     (1 << 16)   // this bit changes every time the data changed

#define TIMEOUT         200         // timeout 1 sec

void cart_cmd(BYTE ReadNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount);

#endif
