#include "lpc13xx.h"

#include <cr_section_macros.h>
#include <NXP/crp.h>

__CRP const unsigned int CRP_WORD = CRP_NO_CRP ;

#include <stdio.h>
#include "defs.h"

void init_timer32(void);
void setupClock(void);

BYTE times[2048];
WORD cnt;

int main(void) {
	setupClock();

	printf("FDD FW\n");

	//	// config PIO2 outputs
	//	WORD *pio2dir = (WORD *) 0x50028000;
	//	*pio2dir = INDEX | TRACK0 | RDATA | WR_PROTECT | DISK_CHANGE;
	//
	//	// config PIO3 outputs
	//	WORD *pio3dir = (WORD *) 0x50038000;
	//	*pio3dir = ATN;

	// GPIO3IS  is 0 after reset = edge sensitive
	// GPIO3IEV is 0 after reset = interrupt on falling edge
	//	WORD *GPIO3RIS = (WORD *) 0x50038014;		// GPIO raw interrupt status register -- read to get interrupt status
	//	WORD *GPIO3IC  = (WORD *) 0x5003801C;		// GPIO interrupt clear register      -- write 1 to clear interrupt bit

	//----------
	// need to enable clock of IOCON to make IOCON work
	LPC_SYSCON->SYSAHBCLKCTRL |= (1<<16);			// enable IOCON clock

	init_timer32();

	cnt = 0;

	while(1) {
		WORD val = LPC_TMR32B0->IR;

		if(val & 0x10) {				// is CR0INT set?
			LPC_TMR32B0->IR = 0x10;		// clear CR0INT

			val = LPC_TMR32B0->CR0;		// read CR0

			if(cnt < 2048) {			// if don't have 2048 values
				times[cnt] = val;		// store
				cnt++;					// move to next slot
			} else {
				printf("done\n");
			}

			LPC_TMR32B0->TCR = 0x03;				// enable TC and reset TC value
			LPC_TMR32B0->TCR = 0x01;				// enable TC and no reset anymore
		}

	}
	return 0 ;
}

void init_timer32(void)
{
	// Some of the I/O pins need to be carefully planned if you use below module because JTAG and TIMER CAP/MAT pins are muxed.

	LPC_SYSCON->SYSAHBCLKCTRL |= (1<<9);	// enable clock for CT32B0

	LPC_IOCON->PIO1_5 &= ~0x07;				// remove 3 lowest bits of this IOCON
	LPC_IOCON->PIO1_5 |=  0x02;				// set function of PIO1_5 to Timer0_32 CAP0

	// LPC_TMR32B0->CTCR = 0 after reset; that is -- Timer Mode: every rising PCLK edge

	LPC_TMR32B0->CCR = 0x06;				// Capture on CT32Bn_CAP0 falling edge, Interrupt on CT32Bn_CAP0 event

	// LPC_TMR32B0->PR  -- prescale, 0= TC++ on every PCLK, 1= TC++ on every 2 PCLKs, ...
	LPC_TMR32B0->PR = 35;					// prescale of TC++. When CLK is 72 MHz and prescale is 35, it should prescale TC++ to 0.5 us

	// LPC_TMR32B0->IR  -- bit 4 - CR0INT - Interrupt flag for capture channel 0 event.
	// LPC_TMR32B0->CR0 -- Capture Register. CR0 is loaded with the value of TC when there is an event on the CT32B0_CAP0 input.

	LPC_TMR32B0->TCR = 0x01;				// enable TC
}

void setupClock(void)
{
	LPC_SYSCON->SYSOSCCTRL = 0;			// 1-20MHz range + Oscillator not bypassed
	LPC_SYSCON->PDRUNCFG &= ~(1<<5);	// power up system oscillator
	LPC_SYSCON->SYSPLLCLKSEL = 1;		// System oscillator is input for the PLL
	LPC_SYSCON->SYSPLLCLKUEN = 0;		// clear SYSPLLCLKUEN
	LPC_SYSCON->SYSPLLCLKUEN = 1;		// start pll input clock change
	LPC_SYSCON->SYSPLLCTRL = 0x25;		// PLL P=2, M=6 for CLKIN x 6 = 72 MHz
	LPC_SYSCON->PDRUNCFG &= ~(1<<7);	// power up system pll

	while(!(LPC_SYSCON->SYSPLLSTAT & 0x01));	// Wait for PLL to lock

	LPC_SYSCON->MAINCLKSEL = 3;			// main clock = system pll clock out
	LPC_SYSCON->MAINCLKUEN = 0;			// clear MAINCLKUEN
	LPC_SYSCON->MAINCLKUEN = 1;			// start main clock change to pll output

	LPC_SYSCON->SYSAHBCLKDIV = 1;		// core/peripheral clock = main clock divided by 1
}
