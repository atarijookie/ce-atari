//---------------------------------------
#include <defBF531.h>

//---------------------------------------
.section L1_code;

.global _Delay110ns;
.type _Delay110ns,STT_FUNC; 
.align 4; 

_Delay110ns:
	[--sp] = p0;				// push regs to stack
	[--sp] = lc0;
	[--sp] = lt0;
	[--sp] = lb0;
	
	p0 = 29 (Z);				// 44 instructions @ 400 MHz = 110 ns (15 instructions are after and before the loop)

	nop;						// these NOPs are here to avoid 'read after write'
	nop;
	nop;
	nop;

	LSETUP(Delay110nsloop, Delay110nsloop) lc0 = p0;

Delay110nsloop:	nop;

	lb0 = [sp++];
	lt0 = [sp++];
	lc0 = [sp++];
	p0 = [sp++];				// pop regs from stack
	rts;						// return 
_Delay110ns.end:
//---------------------------------------

.global _Delay300ns;
.type _Delay300ns,STT_FUNC; 
.align 4; 

_Delay300ns:
	[--sp] = p0;				// push regs to stack
	[--sp] = lc0;
	[--sp] = lt0;
	[--sp] = lb0;

	p0 = 105 (Z);				// 120 instructions @ 400 MHz = 300 ns (15 instructions are after and before the loop)

	nop;					// these NOPs are here to avoid 'read after write'
	nop;
	nop;
	nop;

	LSETUP(Delay300nsloop, Delay300nsloop) lc0 = p0;

Delay300nsloop:	nop;

	lb0 = [sp++];
	lt0 = [sp++];
	lc0 = [sp++];
	p0 = [sp++];				// pop regs from stack
	rts;						// return 
_Delay300ns.end:
//---------------------------------------

