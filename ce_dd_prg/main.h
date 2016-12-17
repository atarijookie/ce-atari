#ifndef _MAIN_H_
#define _MAIN_H_


#define DMA_BUFFER_SIZE		512

/* TODO : use the system provided buffer through _FRB Cookie
 * http://toshyp.atari.org/en/003007.html#Cookie_2C_20_FRB
 * this is a 64 KByte buffer in ST-RAM that can be used by an
 * Atari TT for ACSI-DMA transfers.
 * access is coordinated via the system variable flock.
 *
 * We should also study the XFRB cookie provided by HDDriver
 * http://toshyp.atari.org/en/003007.html#Cookie_2C_20XFRB
 */

// If you intend to change FASTRAM_BUFFER_SIZE,
// also change the .comm FastRAMBuffer value directly below
//
// (thanks C for not not providing a clear interface between
// header files and assembly ;))
//
// (You might wonder "why use .balign and not define a buffer
// somewhere using the aligned((2)) attribute? Well, then gcc
// during compiling will give a friendly message like this:
// warning: alignment of 'FastRAMBuffer' is greater than maximum object file alignment.  Using 2 [enabled by default]
// So, .balign then. But wait, this only works with assembly
// source code! You can guess the rest :))
#define FASTRAM_BUFFER_SIZE	4096
__asm__ (".balign 4\n\t"
		 ".comm _FastRAMBuffer,4096");

#define _longframe      ((volatile WORD *) 0x59e)

typedef struct {
    BYTE enabled;
    BYTE take;
} ScreenShots;

#endif
