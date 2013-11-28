| Simple GEMDOS handler.
| 26/11/2013 Miro Kropacek
| miro.kropacek@gmail.com

	.globl	_gemdos_handler
	.globl	_gemdos_table
	.globl	_old_gemdos_handler
	.globl	_useOldHandler

| ------------------------------------------------------
	.text
| ------------------------------------------------------

	.ascii	"XBRA"
	.ascii	"CEDD"
_old_gemdos_handler:
	.long	0

| GEMDOS call looks on the stack like this:
| paramN
| :
| param2
| param1
| function number
| return address (long)
| stack frame (word)	<--- (sp)

_gemdos_handler:
	tst.w	_useOldHandler
	bne.b	gemdos_not_handled
	
	lea	2+4(sp),a0				| a0 points to the function number now
	btst.b	#5,(sp)					| check the S bit in the stack frame
	bne.b	gemdos_call
	move	usp,a0					| if not called from SV, take params from the user stack
gemdos_call:
	lea	_gemdos_table,a1
	move.w	(a0)+,d0				| fn
	cmp.w	#0x100,d0				| number of entries in the function table
	bhs.b	gemdos_not_handled			| >=0x100 are MiNT functions

	add.w	d0,d0					| fn*4 because it's a function pointer table
	add.w	d0,d0					|
	adda.w	d0,a1
	tst.l	(a1)
	beq.b	gemdos_not_handled
	movea.l	(a1),a1

	movem.l	d2-d7/a2-a6,-(sp)
	move.l	a0,-(sp)				| param #1: stack pointer with function params
	jsr	(a1)					| call the handler
	addq.l	#4,sp
	movem.l	(sp)+,d2-d7/a2-a6
	rte						| return from exception, d0 contains return code
	
gemdos_not_handled:
	move.l	_old_gemdos_handler(pc),-(sp)		| Fake a return
	rts						| to old code.

