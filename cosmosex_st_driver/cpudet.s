|
| Crude CPU detect routine
| George Nakos 13 Aug 2014
| ggn.dbug@gmail.com
|

	.globl	_cpudet
	
_cpudet:

	clr.w	_trap_extra_offset		|assume 68000
	move.l	0xc.w,a0				|save old address error vector
	move.l	#ouch,0xc.w				|install our own vector temporarily
	move.l	sp,a1					|save stack value
	move.w	_trap_extra_offset+1,d0	|a 68000 will crash at this point and skip the following instruction
	move.w	#2,_trap_extra_offset	|a 68030 will continue normally, so set the extra offset to 2
ouch:
	move.l	a0,0xc.w				|done being naughty, restore address vector
	move.l	a1,sp					|restore stack value
	
	rts								|byee!
	
	
.bss
	.globl	_trap_extra_offset
_trap_extra_offset:
	ds.w 1							|this is 0 for 68000 and 2 for 68030. See gemdos_asm.s and bios_asm.s
	