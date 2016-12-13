#ifndef _FIND_CE_H_
#define _FIND_CE_H_

#define MACHINE_ST      0
#define MACHINE_TT      2
#define MACHINE_FALCON  3

BYTE findDevice(void);

#define MACHINECONFIG_HAS_STE_SCRADR_LOWBYTE 1
#define MACHINECONFIG_HAS_VIDEL 			 2
#define MACHINECONFIG_HAS_TT_VIDEO           4
#define MACHINECONFIG_HAS_BLITTER            8

#endif

