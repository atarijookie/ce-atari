| Simple GEMDOS handler.
| 04/03/2014 Miro Kropacek
| miro.kropacek@gmail.com

	.globl	_gemdos_handler
	.globl	_gemdos_table
	.globl	_old_gemdos_handler
	.globl	_useOldGDHandler

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
	tst.w	_useOldGDHandler
	bne.b	gemdos_not_handled
	
	lea     2+4(sp),a0				| a0 points to the function number now
	btst.b	#5,(sp)					| check the S bit in the stack frame
	bne.b	gemdos_call
	move	usp,a0					| if not called from SV, take params from the user stack

gemdos_call:
	lea     _gemdos_table,a1
	move.w	(a0)+,d0				| fn
	cmp.w	#0x100,d0				| number of entries in the function table
	bhs.b	gemdos_not_handled		| >=0x100 are MiNT functions

   	cmp.w	#0x004b,d0				| Pexec()?
	bne.b	ok					    | no -> proceed as usual
	cmpi.w	#0,(a0)					| PE_LOADGO?
	beq.b	ok					    | yes -> load by us, execute by the OS
	cmpi.w	#3,(a0)					| PE_LOAD?
	bne.b	gemdos_not_handled		| yes -> load by us, no -> all the others, don't mess with us
    
ok:    
	add.w	d0,d0					| fn*4 because it's a function pointer table
	add.w	d0,d0					|
	adda.w	d0,a1
	tst.l	(a1)
	beq.b	gemdos_not_handled
	movea.l	(a1),a1

	move.l	a0,-(sp)				| param #1: stack pointer with function params
	jsr     (a1)					| call the handler
	addq.l	#4,sp
	rte                             | return from exception, d0 contains return code
	
gemdos_not_handled:
    clr.w   _useOldGDHandler                | ensure that is our handler still alive after the call (which may not return) 
	move.l  _old_gemdos_handler(pc),-(sp)	| Fake a return
	rts		                                | to old code.

