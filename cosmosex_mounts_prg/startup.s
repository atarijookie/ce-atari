	.globl	_main
	.globl	___main

| ------------------------------------------------------	
	.text
| ------------------------------------------------------

	move.l	#__stack,a0				| get stack address
	move.l	#0x1000, d0				
	add.l	a0,d0					| move to the end of stack
	and.b	#0xf0,d0				| align stack
	move.l	d0,sp					| new stackspace

	jsr		_main

	clr.w	-(sp)
	trap	#1

___main:
	rts

| ------------------------------------------------------
	.bss
| ------------------------------------------------------	
__stack:				.ds.l	4200
