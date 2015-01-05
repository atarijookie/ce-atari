| Simple BIOS handler.
| 04/03/2014 Miro Kropacek
| miro.kropacek@gmail.com
| Small fixes: George Nakos 2014
| ggn.dbug@gmail.com

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
	
	lea		6(sp),a0				| a0 points to the function number for 68000
	add.w	_trap_extra_offset,a0	| a0 points to the function number for 68000/68030 now
									| (we have to do this becuase the 68030 stack frame is
									|  different than the 68000, 6 bytes vesus 8. So we add
									|  either 0 or 2)
	btst.b	#5,(sp)					| check the S bit in the stack frame
	bne.b	bios_call
	move	usp,a0					| if not called from SV, take params from the user stack
bios_call:
	move.w	(a0)+,d0				| fn
	cmp.w	#0x100,d0				| number of entries in the function table
	bhs.b	bios_not_handled

	add.w	d0,d0					| fn*4 because it's a function pointer table
	add.w	d0,d0					|
	ext.l	d0
	add.l	#_bios_table,d0
	
	exg		a0,d0
	tst.l	(a0)
	beq.b	bios_not_handled
	movea.l	(a0),a0

	move.l	d0,-(sp)				| param #1: stack pointer with function params
	jsr		(a0)					| call the handler
	addq.l	#4,sp
	rte								| return from exception, d0 contains return code
	
bios_not_handled:
	clr.w	_useOldBiosHandler              | ensure that is our handler still alive after the call (which may not return) 
	move.l	_old_bios_handler(pc),-(sp)		| Fake a return
	rts								        | to old code.

	.bss
