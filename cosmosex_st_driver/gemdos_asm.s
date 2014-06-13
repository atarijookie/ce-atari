| Simple GEMDOS handler
| Miro Kropacek, 2013 & 2014
| miro.kropacek@gmail.com

	.globl	_gemdos_handler
	.globl	_gemdos_table
	.globl	_old_gemdos_handler
	.globl	_useOldGDHandler
	.globl	_pexec_postProc
    .globl  _pexec_callOrig
    
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
	bne.b	callOriginalHandler
	
	lea     2+4(sp),a0				        | a0 points to the function number now
	btst.b	#5,(sp)					        | check the S bit in the stack frame
	bne.b	gemdos_call
	move	usp,a0					        | if not called from SV, take params from the user stack

gemdos_call:
	move.w	(a0)+,d0				        | fn
	cmp.w	#0x100,d0				        | number of entries in the function table
	bhs.b	callOriginalHandler		        | >=0x100 are MiNT functions

   	cmp.w   #0x004b,d0                      | Pexec()?
	bne.b   callCustomHandlerNotPexec       | not Pexec(), just call the custom handler

    bra     callCustomHandlerForPexec


|============================================    
| This short piece of code calls the original GEMDOS handler.    

callOriginalHandler:
    clr.w   _useOldGDHandler                | ensure that is our handler still alive after the call (which may not return).
	move.l  _old_gemdos_handler(pc),-(sp)	| Fake a return
	rts		                                | to old code.

    
|============================================    
| Following code calls custom handler for any GEMDOS function other than Pexec() (they don't need any special processing).

callCustomHandlerNotPexec:                  |    
	lea     _gemdos_table,a1                | get the pointer to custrom handlers table
	add.w   d0,d0                           | fn*4 because it's a function pointer table
	add.w   d0,d0                           |
	adda.w  d0,a1
	
    tst.l   (a1)
	beq.b   callOriginalHandler             | if A1 == NULL, we don't have custom handler and use original handler
	
    movea.l (a1),a1
	move.l  a0,-(sp)                        | param #1: stack pointer with function params
	jsr     (a1)                            | call the custom handler

    addq.l  #4,sp
    rte                                     | return from exception, d0 contains return code

    
|============================================    
callCustomHandlerForPexec:    
	cmpi.w  #0,(a0)					        | PE_LOADGO?
	beq.b   handleThisPexec 		        | this is PE_LOADGO, handle it!

	cmpi.w  #3,(a0)					        | PE_LOAD?
	beq.b   handleThisPexec 		        | this is PE_LOAD, handle it!

    bra.b   callOriginalHandler             | this is not PE_LOADGO and PE_LOAD, so call the original Pexec()
    
handleThisPexec:                            
| If we got here, it's Pexec() with PE_LOADGO or PE_LOAD.
| In the C part set the pexec_postProc to non-zero at the end of the handler 
| *AFTER* you called any GEMDOS functions, and it will call the original Pexec() with PE_GO param.

	lea     _gemdos_table,a1                | get the pointer to custrom handlers table
	add.w   d0,d0                           | fn*4 because it's a function pointer table
	add.w   d0,d0                           |
	adda.w  d0,a1
    
	tst.l   (a1)
	beq.b   callOriginalHandler             | if we don't have custom (Pexec) handler, call original handler
    
	movea.l (a1),a1
	move.l  a0,-(sp)                        | param #1: stack pointer with function params
	jsr     (a1)                            | call the custom handler

    tst.w   _pexec_callOrig                 | if this is set to non-zero, call original handler now - when the C code didn't handle it and we should try it with original handler.
    bne.b   callOriginalPexec
    
	tst.w   _pexec_postProc                 | should we now do the post-process of PE_LOADGO by calling PE_GO?
	bne.b   callPexecGo                     | yes, call Pexec()
	
    addq.l  #4,sp
    rte                                     | return from exception, d0 contains return code
    
callPexecGo:    
| The original call was Pexec(PE_LOADGO), the Pexec(PE_LOAD) part was done in C code.
| Now this asm part will call Pexec(PE_GO), the basepage parameter is in d0 already.
|
| Transform from: int32_t Pexec (0, int8_t *name, int8_t *cmdline, int8_t *env);
|             to: int32_t Pexec (4, 0L, PD *basepage, 0L);
	
	movea.l	(sp)+,a0                        | restore the SP and param pointer
	move.w  #4,(a0)+
	clr.l   (a0)+
	move.l  d0,(a0)+
	clr.l   (a0)+

	clr.w   _pexec_postProc                 | clear the PE_LOADGO flag
	bra.b   callOriginalHandler             | now do the PE_GO part
    |------------------
	
callOriginalPexec:    
    movea.l	(sp)+,a0                        | restore the SP and param pointer
    clr.w   _pexec_callOrig                 | clear flag that original pexec should be called
    bra     callOriginalHandler             | ...and jump to old handler
    

|============================================     
    
	.data
_pexec_postProc:    dc.w    0       | 1 = post-process PE_LOADGO
_pexec_callOrig:    dc.w    0       | 1 = after custom handler call original Pexec (no post processing)
