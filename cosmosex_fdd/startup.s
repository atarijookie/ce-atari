	.globl	_main
	.globl	__base
	.globl	___main
	.globl  __runFromBootsector

| ------------------------------------------------------	
	.text
| ------------------------------------------------------
	move.l	-4(sp),d0				| address to basepage

	cmp.l	#0x12345678, d0			| if found a magic number then we're called from bootsector?
	beq		fromBootsector			| skip Mshrink, terminate with rts

| the following code should run when driver was run from TOS (not bootsector)
	move.l	#0,__runFromBootsector	| mark that the app was run from TOS 

	move.l	4(sp),a5				| address to basepage
	move.l	0x0c(a5),d0				| length of text segment
	add.l	0x14(a5),d0				| length of data segment
	add.l	0x1c(a5),d0				| length of bss segment
	add.l	#0x1000+0x100,d0		| length of stack+basepage
	move.l	a5,d1					| address to basepage
	move.l	a5,__base				|
	add.l	d0,d1					| end of program
	and.b	#0xf0,d1				| align stack
	move.l	d1,sp					| new stackspace

	move.l	d0,-(sp)				| mshrink()
	move.l	a5,-(sp)				|
	clr.w	-(sp)					|
	move.w	#0x4a,-(sp)				|
	trap	#1						|
	lea.l	12(sp),sp				|

	jsr		_main

	clr.w	-(sp)
	trap	#1

___main:
	rts

	
| the following code should run when driver was run from bootsector (not TOS)
	
fromBootsector:
	move.l	#1,__runFromBootsector	| mark that the app was run from bootsector 

	jsr	_main
	
	rts

| ------------------------------------------------------
	.bss
| ------------------------------------------------------	
__base:					.ds.l	1
__runFromBootsector:	.ds.l	1

