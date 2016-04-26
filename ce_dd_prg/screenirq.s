| some helper routines to handle the VBL, frame skipping and calling of worker routines

    .globl  _init_screencapture
    .globl  _screenworker
    
    .text 

_vblqueue   = 0x456 |.l
_nvbls      = 0x454 |.w
_flock      = 0x43e |.w
_v_bas_ad   = 0x44e |.l

_init_screencapture:
    movem.l D0-A6,-(SP)
    
    move.l  0x70,__savevbl
    move.l  #screenirq,0x70                     | install VBL
	clr.w 	__vblcntscreen   

    movem.l (SP)+,D0-A6
    rts

| -------------------------------------------------    
screenirq:
    movem.l D0-A6,-(SP)
    | --------------------
    | this limits the screen shot to once per second
    lea     __vblcntscreen,A1
    addq.w  #1,(A1)                             | increment vbl cnt
    move.w  (A1),D0                             | get vbl cnt  
    cmp.w   #50,D0                              
    blt.s   screenirq_end                       | do nothing if not at least 50 VBLs have elapsed

    | --------------------
    | try to acquire FLOCK if it's not set, and if it's used by someone else, skip screenshot cycle
    move.w  _flock,-(SP)                        | save flock

    move.w  sr,-(SP)
    move    #0x2700,sr                          | protect this from interrupts, making it almost atomic  
    
    tst.w   _flock                              | unfortunately we can't use tas, as the _flock word is perused by different drivers differently - TOS sets 1.w, other only st the first byte etc.
    bne.s   castskipflock                       | don't do anything if flock is set - we can't use the DMA then, anyways
    
    move.w  #-1,_flock                          | keep others from using DMA  
    move.w  (SP)+,sr
    | --------------------

    clr.w   (A1)                                | reset skip-counter for next frameskip

    lea     _screenworker,A0
    jsr     (A0)                                | call worker

    move.w  (SP)+,_flock                        | set flock to previous state
    jmp     screenirq_end
    
castskipflock:
    move.w  (SP)+,sr
    move.w  (SP)+,_flock                        | set flock to previous state
    
screenirq_end:    
    movem.l (SP)+,D0-A6
    move.l  __savevbl,-(SP)                     | call original VBL routine   
    rts
| -------------------------------------------------    

    .bss
__vblcntscreen:         .ds.w   1
__savevbl:              .ds.l   1

