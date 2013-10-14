//-------------------------------------------
#include <defBF531.h>
//-------------------------------------------
	.section program;
	.align 2;

	.extern _SetupClocks;
	.type _SetupClocks,STT_FUNC;

_SetupClocks:
	[--sp] = r0;				// push regs to stack
	[--sp] = r1;
	[--sp] = r2;
	[--sp] = p0;
	
	//-----------
	p0.h = HI(SIC_IWR);			// PLL WAKEUP enable
	p0.l = LO(SIC_IWR);
	r2 = 0x0001 (Z);
	csync;
	[p0] = r2;
	//-----------
	p0.h = HI(PLL_LOCKCNT);		// LOCKCNT = 512
	p0.l = LO(PLL_LOCKCNT);
	r2 = 0x0200 (Z);
	csync;
	w[p0] = r2;
	//-----------
	p0.h = HI(PLL_DIV);			// CCLK = VCO, SCLK = VCO/3
	p0.l = LO(PLL_DIV);
	r2 = 0x0003 (Z);
	csync;
	w[p0] = r2;
	//-----------
	p0.h = HI(PLL_CTL);			// VCO = CLKIN * 20 (20 * 20 MHz)
	p0.l = LO(PLL_CTL);
//	r2 = 0x2800 (Z);			// MSEL = 20 (20 * 20 MHz = 400 MHz)
	r2 = 0x3200 (Z);			// MSEL = 25 (25 * 16 MHz = 400 MHz)
//	r2 = 0x5000 (Z);			// MSEL = 40 (40 * 10 MHz = 400 MHz)
	csync;
	//-----------
	cli r0;						// dissable interrupts
	w[p0] = r2;					// write the new PLL_CTL
	
	idle;						// go IDLE and wait for PLL WAKEUP
	sti r0;							
	//-----------
	p0 = [sp++];				// pop regs from stack
	r2 = [sp++];
	r1 = [sp++];
	r0 = [sp++];
	
	rts;
_SetupClocks.end:
//-------------------------------------------
	.extern _TestDRQ;
	.type _TestDRQ,STT_FUNC;

_TestDRQ:
	[--sp] = r0;				// push regs to stack
	[--sp] = r1;
	[--sp] = r2;
	[--sp] = p0;
	
	//-----------
	p0.h = HI(FIO_FLAG_C);
	p0.l = LO(FIO_FLAG_C);

	p1.h = HI(FIO_FLAG_S);
	p1.l = LO(FIO_FLAG_S);
	
	r0 = 0x0002 (Z);
	//-----------
DeadLock:	
	r1 = 7 (Z);
	w[p0] = r0;				// set
	
Wait1:
	nop;
	nop;
	nop;
	nop;
	nop;

	r1 += -1;
	
	cc = r1 == 0;
	if !cc jump Wait1;
	//-----------
	r1 = 150 (Z);
	w[p1] = r0;				// clear
	
Wait2:
	nop;
	nop;
	nop;
	nop;
	nop;

	r1 += -1;

	cc = r1 == 0;
	if !cc jump Wait2;
	//-----------
	jump DeadLock;	
	
	p0 = [sp++];				// pop regs from stack
	r2 = [sp++];
	r1 = [sp++];
	r0 = [sp++];
	
	rts;
_TestDRQ.end:
//-------------------------------------------
