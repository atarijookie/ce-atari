	.globl	_main
	.globl	__base
	.globl	___main

| ------------------------------------------------------	
	.text
| ------------------------------------------------------
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

   	move.l	__base,a0				| address to basepage
    add.l   #0x80,a0                | offset 0x80: pointer to cmd line
 
    move.l  a0,-(sp)                | argv
    move.l  #1,-(sp)                | argc
 
	jsr		_main

    add.l   #8, sp
    
	clr.w	-(sp)
	trap	#1

___main:
	rts

	
| ------------------------------------------------------
	.bss
| ------------------------------------------------------	
__base:					.ds.l	1

