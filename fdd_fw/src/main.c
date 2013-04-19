#include "lpc13xx.h"

#include <stdio.h>
#include "defs.h"

void init_timer32(void);
void setupClock(void);
void printit(void);
void appendBit(BYTE bit);
void appendChange(BYTE change);
void addAttention(BYTE atn);
void init_timer_index(void);
void init_timer_rdata(void);
void getNextMfmTime(void);

// worst case scenario: 1 sector with all bullshit encoded in MFM should be max. 1228 bytes
// So a 4096 bytes big buffer should contain at least 3.3 sectors (one currently streamed, one received from host + something more)

// gap byte 0x4e == 666446 us = 222112 times = 10'10'10'01'01'10 = 0xa96
// 2 gap bytes 0x4e 0x4e = 0xa96a96 times = 0xa9 0x6a 0x96 bytes

BYTE mfm[4096];
WORD mfmIndexAdd, mfmIndexGet, mfmCount;
BYTE mfmByte, mfmByteIndex;

BYTE newByte;
BYTE newBits;

BYTE changes;
BYTE chCount;

DWORD sync;
BYTE scnt;

#define CMD_TRACK_CHANGED       1               // +  track # + side (highest bit)
#define CMD_SEND_NEXT_SECTOR    2               // +  track # + side (highest bit)
#define CMD_SECTOR_WRITTEN      3               // + sector # + side (highest bit)
#define CMD_BUTTON_PRESSED      4

#define MFM_4US     1
#define MFM_6US     2
#define MFM_8US     3

void mfm_append_change(BYTE change);

// - viem zapisanim do TMR16B0EMR nastavit hodnotu EM0 a EM1?
// - check a obnova rdata timeru musi zbehnut do 260 cyklov (tolko je pre 4 us) -- of jedneho po druhe volanie getNextMfmTime()
// - write support

int main(void) {
	setupClock();

	printf("FDD FW\n");

	// config PIO2 outputs
	WORD *pio2dir = (WORD *) 0x50028000;
	*pio2dir = 0;							// all pins as inputs, drive not selected

	//	// config PIO3 outputs
	//	WORD *pio3dir = (WORD *) 0x50038000;
	//	*pio3dir = ATN;

	// GPIO3IS  is 0 after reset = edge sensitive
	// GPIO3IEV is 0 after reset = interrupt on falling edge
	//	WORD *GPIO3RIS = (WORD *) 0x50038014;		// GPIO raw interrupt status register -- read to get interrupt status
	//	WORD *GPIO3IC  = (WORD *) 0x5003801C;		// GPIO interrupt clear register      -- write 1 to clear interrupt bit

	WORD *index		= (WORD *) 0x50020200;			// PIO2_7 - INDEX
	WORD *track0	= (WORD *) 0x50020400;			// PIO2_8 - TRACK0
	WORD *rdata		= (WORD *) 0x50020800;			// PIO2_9 - RDATA
	WORD *wr_prt	= (WORD *) 0x50021000;			// PIO2_10 - WRITE_PROTECT
	WORD *dskchg	= (WORD *) 0x50022000;			// PIO2_11 - DISK_CHANGE

	WORD *in = (WORD *) 0x500201FC;				// floppy input pins

	WORD *GPIO2RIS	= (WORD *) 0x50028014;		// GPIO raw interrupt status register -- read to get interrupt status
	WORD *GPIO2IC	= (WORD *) 0x5002801C;		// GPIO interrupt clear register      -- write 1 to clear interrupt bit
	//----------
	// need to enable clock of IOCON to make IOCON work
	LPC_SYSCON->SYSAHBCLKCTRL |= (1<<16);			// enable IOCON clock

	init_timer32();

	init_timer_index();
	init_timer_rdata();

	// init circular buffer for streamed data
	mfmIndexAdd		= 0;
	mfmIndexGet		= 0;
	mfmCount		= 0;

	mfmByte			= 0;
	mfmByteIndex	= 0;

	// init track and side vars for the floppy position
	BYTE track = 0;
	BYTE side, prevSide = 0;

	// init floppy signals
	*wr_prt = WR_PROTECT;						// not write protected
	*dskchg = DISK_CHANGE;						// not changing disk (ready!)

	BYTE outputsActive = 0;

	while(1) {
		WORD inputs = *in;										// read floppy inputs

		// TODO: add reading of SPI here

		if((inputs & (MOTOR_ENABLE | DRIVE_SELECT)) != 0) {		// motor not enabled, drive not selected? do nothing
			if(outputsActive == 1) {							// if we got outputs active
				*pio2dir = 0;									// all pins as inputs, drive not selected
				outputsActive = 0;
			}

			continue;
		}

		if(outputsActive == 0) {								// if the outputs are inactive
			*pio2dir = INDEX | TRACK0 | RDATA | WR_PROTECT | DISK_CHANGE;	// set these as outputs
			outputsActive = 1;
		}

		WORD tmr1Ints = LPC_TMR16B1->IR;						// read TMR16B1, which we to create MFM stream
		if(tmr1Ints & 2) {										// is MR1INT set? we streamed out one time
			LPC_TMR16B1->IR = 2;								// clear MR1INT
			getNextMfmTime();									// get the next time from stream, set up TMR16B1
		}

		// check for STEP pulse - should we go to a different track?
		WORD ints = *GPIO2RIS;									// read interrupt status of GPIO2
		if(ints & STEP) {										// if falling edge of STEP signal was found
			*GPIO2IC = STEP;									// clear that int

			if(inputs & DIR) {									// direction is High? track--
				if(track > 0) {
					track--;

					addAttention(CMD_TRACK_CHANGED);
				}
			} else {											// direction is Low? track++
				if(track < 82) {
					track++;

					addAttention(CMD_TRACK_CHANGED);
				}
			}

			if(track == 0) {									// if track is 0
				*track0 = 0;									// TRACK 0 signal to L
			} else {											// if track is not 0
				*track0 = TRACK0;								// TRACK 0 signal is H
			}
		}

		//------------
		// update SIDE var
		side = (inputs & SIDE1) ? 0 : 1;						// get the current SIDE
		if(prevSide != side) {									// side changed?
			addAttention(CMD_SEND_NEXT_SECTOR);					// we need another sector, this time from the right side!
			prevSide = side;
		}

		//------------
		// generate INDEX pulse as needed
		WORD tmr0Ints = LPC_TMR16B0->IR;						// read TMR16B0, which we use for index pulse generation
		if(tmr0Ints & 1) {										// is MR0INT set? (195 ms elapsed?)
			LPC_TMR16B0->IR = 1;								// clear MR0INT
			*index = 0;											// index pulse low


			// TODO: TRACK started! init code needed


		}

		if(tmr0Ints & 2) {										// is MR1INT set? (200 ms elapsed?)
			LPC_TMR16B0->IR = 2;								// clear MR0INT
			*index = INDEX;										// index pulse high
		}
	}

//	cnt = 0;
//
//	newByte	= 0;
//	newBits	= 0;
//	sync	= 0;
//	scnt	= 0;
//
//	changes = 0;
//	chCount = 0;
//
//	BYTE spare = 0;
//
//	DWORD pval, cval, dval;
//	pval = 0;
//
//	while(1) {
//		WORD val = LPC_TMR32B0->IR;		// get interrupt status
//
//		if(val & 0x10) {				// is CR0INT set?
//			LPC_TMR32B0->IR = 0x10;		// clear CR0INT
//
//			cval = LPC_TMR32B0->CR0;	// read CR0
//
//			dval = cval - pval;
//
//			val = dval;
//			pval = cval;
//
//			if(cnt == 7000) {
//				printf("done\n");
//				printit();
//			}
//
//			//--------------
//			if(val < 200 || val > 720) {	// bellow 2.7 us or above 10 us - ignore
//				continue;
//			}
//
//			if(val < 360) {			// 4 is bellow 360
//				val = 4;			// 4 us = RN
//
//				if(spare) {			// input is RN, we got spare N, this creates NRN - it means NR and spare N
//					appendBit(1);	// NR = 1, still got spare N
//				} else {			// no spare N
//					appendBit(0);	// RN = 0, still no spare
//				}
//			} else if(val < 500) {	// 6 is above 360 and bellow 500
//				val = 6;			// 6 us = RNN
//
//				if(spare) {			// input is RNN, we got spare N, this creates NRNN - it means NR and NN
//					appendBit(1);	// NR = 1
//					appendBit(0);	// NN = 0
//					spare = 0;		// no spare anymore
//				} else {			// no spare N, we got RNN, this is RN and spare N
//					appendBit(0);	// RN = 0
//					spare = 1;		// last N is spare
//				}
//			} else {				// 8 is above 500
//				val = 8;			// 8 us = RNNN
//
//				if(spare) {			// input is RNNN, we got spare N, this creates NRNNN - it means NR, NN and spare N
//					appendBit(1);	// NR = 1
//					appendBit(0);	// NN = 0
//					// still one spare N
//				} else {			// no spare N, we got RNNN, this is RN and NN
//					appendBit(0);	// RN = 0
//					appendBit(0);	// NN = 0
//					// still no spare N
//				}
//			}
//
//			sync = sync << 8;
//			sync = sync | ((DWORD) val);
//
//			if(sync == 0x08060806) {
//				scnt++;
//				sync = 0;
//
//				newBits = 7;
//				spare = 1;
//
//				chCount = 1;						// mark that we got half of the last bit - this might fuck up decoded sync mark when not in sync before
//			}
//		}
//	}

	return 0 ;
}

void getNextMfmTime(void)
{
	// 0.4 us = 28 cycles (pulse low width)
	// 4 us = 288 cycles  (260 cycles)
	// 6 us = 432 cycles  (404 cycles)
	// 8 us = 576 cycles  (548 cycles)

	static BYTE gap[3] = {0xa9, 0x6a, 0x96};
	static BYTE gapCnt = 0;

	BYTE time = mfmByte >> 6;								// get only 2 highest bits

	mfmByte = mfmByte << 2;									// move the mfmByte to next position
	mfmByteIndex++;

	WORD mr0, mr1;

	switch(time) {											// convert time code to timer match values
	case MFM_8US:											// 8 us?
		mr0 = 548;
		mr1 = 576;
		break;

	case MFM_6US:											// 6 us?
		mr0 = 404;
		mr1 = 432;
		break;

	case MFM_4US:											// 4 us?
	default:
		mr0 = 260;
		mr1 = 288;
		break;
	}

	LPC_TMR16B1->MR0 = mr0;
	LPC_TMR16B1->MR1 = mr1;

	if(mfmByteIndex == 4) {										// streamed all times from byte?
		mfmByteIndex = 0;

		if(mfmCount == 0) {										// nothing to stream?
			mfmByte	= gap[gapCnt];								// stream this GAP byte
			gapCnt++;

			if(gapCnt > 2) {									// we got only 3 gap byte times, go back to 0
				gapCnt = 0;
			}
		} else {												// got data to stream?
			BYTE hostByte;
			hostByte = mfm[mfmIndexGet];						// get byte from host buffer
			mfmIndexGet++;										// increment position for reading in host buffer
			mfmCount--;											// decrement data in cyclic buffer

			// TODO: add checking and handling of some commands from host (e.g. media change)


			mfmByte = hostByte;
		}
	}
}

void addAttention(BYTE atn)
{
	//TODO: signal to host, send command to host


}

//void appendChange(BYTE change)		// 1 means change (R), 0 means no change (N)
//{
//	changes = changes << 1;			// shift up 1 bit
//	changes = changes & 0x03;		// leave only 2 bottom bits
//	changes = changes | change;		// append new change
//
//	chCount++;						// increment stored change count
//
//	if(chCount == 2) {				// if got 2 changes
//		if(changes == 0 || changes == 2) {		// if it's NN or RN
//			appendBit(0);
//		} else if(changes == 1) {				// if it's NR
//			appendBit(1);
//		}
//
//		chCount = 0;
//	}
//}

//void appendBit(BYTE bit)
//{
//	newByte = newByte << 1;
//	newByte = newByte | bit;
//
//	newBits++;
//	newBits = newBits & 7;			// leave only lowest 3 bits
//
//	if(newBits == 0) {
//		data[cnt] = newByte;		// store
//		cnt++;						// move to next slot
//	}
//}

//void printit(void)
//{
////	printf("Found syncs: %d\n", (int)scnt);
//
//	WORD i;
////	for(i=0; i<2048; i += 16) {
//	for(i=0; i<7000; i += 16) {
////		printf("%d\n", (int)data[i]);
//		printf("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
//				data[i+0], data[i+1], data[i+ 2], data[i+ 3], data[i+ 4], data[i+ 5], data[i+ 6], data[i+ 7],
//				data[i+8], data[i+9], data[i+10], data[i+11], data[i+12], data[i+13], data[i+14], data[i+15]);
//	}
//
//	while(1);
//}

void init_timer32(void)
{
	// Some of the I/O pins need to be carefully planned if you use below module because JTAG and TIMER CAP/MAT pins are muxed.

	LPC_SYSCON->SYSAHBCLKCTRL |= (1<<9);	// enable clock for CT32B0

	LPC_IOCON->PIO1_5 &= ~0x07;				// remove 3 lowest bits of this IOCON
	LPC_IOCON->PIO1_5 |=  0x02;				// set function of PIO1_5 to Timer0_32 CAP0

//	LPC_IOCON->PIO1_5 = (1 << 5) | (0x02);				// set function of PIO1_5 to Timer0_32 CAP0

	// LPC_TMR32B0->CTCR = 0 after reset; that is -- Timer Mode: every rising PCLK edge

	LPC_TMR32B0->CCR = 0x06;				// Capture on CT32Bn_CAP0 falling edge, Interrupt on CT32Bn_CAP0 event

	// LPC_TMR32B0->PR  -- prescale, 0= TC++ on every PCLK, 1= TC++ on every 2 PCLKs, ...
//	LPC_TMR32B0->PR = 35;					// prescale of TC++. When CLK is 72 MHz and prescale is 35, it should prescale TC++ to 0.5 us

	// LPC_TMR32B0->IR  -- bit 4 - CR0INT - Interrupt flag for capture channel 0 event.
	// LPC_TMR32B0->CR0 -- Capture Register. CR0 is loaded with the value of TC when there is an event on the CT32B0_CAP0 input.

	LPC_TMR32B0->TCR = 0x01;				// enable TC
}

void init_timer_index(void)
{
	LPC_SYSCON->SYSAHBCLKCTRL |= (1<<7);	// enable clock for CT16B0

	LPC_TMR16B0->PR		=  36000;			// prescale of TC++. When CLK is 72 MHz and prescale is 36000, it should prescale TC++ to 0.5 ms
	LPC_TMR16B0->MCR	= 0x0019;			// MR0 - enable int, MR1 -- reset & enable int

	LPC_TMR16B0->MR0	= 390;				// MATCH 0 on 195 ms, will just make MR0 int         (put index low)
	LPC_TMR16B0->MR1	= 400;				// MATCH 1 on 200 ms, make MR1 int and reset counter (put index high)

	LPC_TMR16B0->TCR = 0x01;				// enable TC
}

void init_timer_rdata(void)
{
	LPC_SYSCON->SYSAHBCLKCTRL |= (1<<8);	// enable clock for CT16B1

	LPC_TMR16B1->PR	= 0;					// no prescale of TC++

	// MR0 captures start of L on R data (it's period (4,6,8 us) - 0.4 us) - no int, no reset, no stop
	// MR1 captures end of L on R data   (it's the period - 4,6,8 us) - int + reset, no stop
	LPC_TMR16B1->MCR = 0x0018;				// MR0 - no action, MR1 -- reset & enable int

	// 4 us = 288 cycles  (260 cycles)
	LPC_TMR16B1->MR0 = 260;
	LPC_TMR16B1->MR1 = 288;

	LPC_TMR16B1->EMR = 0x00f0;				// EMC0, EMC1 -- toggle MAT0, MAT1 on match

	LPC_TMR16B1->TCR = 0x01;				// enable TC
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
