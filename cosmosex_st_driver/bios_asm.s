| Simple BIOS handler.
| 26/11/2013 Miro Kropacek
| miro.kropacek@gmail.com

	.globl	_bios_handler
	.globl	_bios_table
	.globl	_old_bios_handler
	.globl	_useOldBiosHandler

| ------------------------------------------------------
	.text
| ------------------------------------------------------

	.ascii	"XBRA"
	.ascii	"CEDD"
_old_bios_handler:
	.long	0

| BIOS call looks on the stack like this:
| paramN
| :
| param2
| param1
| function number
| return address (long)
| stack frame (word)	<--- (sp)

_bios_handler:
	tst.w	_useOldBiosHandler
	bne.b	bios_not_handled
	
	lea	2+4(sp),a0				| a0 points to the function number now
	btst.b	#5,(sp)					| check the S bit in the stack frame
	bne.b	bios_call
	move	usp,a0					| if not called from SV, take params from the user stack
bios_call:
	lea	_bios_table,a1
	move.w	(a0)+,d0				| fn
	cmp.w	#0x100,d0				| number of entries in the function table
	bhs.b	bios_not_handled

	add.w	d0,d0					| fn*4 because it's a function pointer table
	add.w	d0,d0					|
	adda.w	d0,a1
	tst.l	(a1)
	beq.b	bios_not_handled
	movea.l	(a1),a1

	move.l	a0,-(sp)				| param #1: stack pointer with function params
	jsr	(a1)					| call the handler
	addq.l	#4,sp
	rte						| return from exception, d0 contains return code
	
bios_not_handled:
	move.l	_old_bios_handler(pc),-(sp)		| Fake a return
	rts						| to old code.

