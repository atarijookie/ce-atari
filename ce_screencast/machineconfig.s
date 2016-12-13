    .globl  _machine_check
    .globl  _machineconfig
    
    .text
_machine_check:
	move.l 	_machineconfig,D0
	bsr 	machine_blitter
	move.l 	D0,_machineconfig
	rts

machine_blitter:
	move.l 	0x8.w,oldbuserror
	move.l 	SP,oldsp
	move.l 	#newbuserror,0x8.w
	move.w 	SR,D1
	tst.w 	0xffff8a00.w   	/* throws bus error if no blitter is present */
	or.l 	#8,D0 	/* MACHINECONFIG_HAS_BLITTER = 8 */
newbuserror:
	move.l  oldbuserror,0x8.w
	move.w 	D1,SR
	move.l 	oldsp,SP
	rts

	rte
	
 	.bss
oldbuserror: 	.ds.l 1 	
oldsp: 			.ds.l 1 	
_machineconfig: .ds.l 1
