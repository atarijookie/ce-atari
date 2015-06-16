| some helper routines to handle the VBL, frame skipping and calling of worker routines

    .globl  _init_screencapture
    .globl  __installed
    .globl  __vblskipscreen
    .globl  __vblskipconfig

    .text 

_vblqueue   = 0x456 |.l
_nvbls      = 0x454 |.w
_flock      = 0x43e |.w
_v_bas_ad   = 0x44e |.l
_init_screencapture:
    movem.l D0-A6,-(SP)

|   find empty slot in vblqueue
|   move.w  _nvbls,D0
|   move.l  _vblqueue,A0
|   subq    #1,D0
|vblfindloop:
|   tst.l   (A0)
|   beq.s   vblinstall
|   addq.l  #4,A0
|   dbra    D0,vblfindloop
    
    move.l  0x70,__savevbl
    move.l  #screenirq,0x70                     | install VBL   

|   clr.w   __installed
|   lea     p1,A0
|   jsr     cconws
|   lea     p2,A0
|   jsr     cconws
|   lea     p3,A0
|   jsr     cconws
|   movem.l (SP)+,D0-A6
|   rts

|there is an empty vbl slot, so install worker in there
|vblinstall:
|   move.l  #screenirq,(A0)

    st      __installed

    lea     p1,A0
    jsr     cconws
    lea     p3,A0
    jsr     cconws
    movem.l (SP)+,D0-A6
    rts

screenirq:
    movem.l D0-A6,-(SP)
    
    lea     _screenworker,A0
    lea     __vblcntscreen,A1
    lea     __vblskipscreen,A2
    jsr     castdo                              | send screencast
  
    lea     _configworker,A0
    lea     __vblcntconfig,A1
    lea     __vblskipconfig,A2
    jsr     castdo                              | get CEs config

    movem.l (SP)+,D0-A6
    
    move.l  __savevbl,-(SP)                     | call original VBL routine   
    rts

castdo:
    addq.w  #1,(A1)                             | cnt
    move.w  (A1),D0                             | cnt  
    cmp.w   (A2),D0                             | skip
    blt.s   castskipvbl                         | do nothing if not at least _vblskip* VBLs have elapsed

    move.w  _flock,-(SP)                        | save flock

    move.w  sr,-(SP)
    move    #0x2700,sr                          | protect this from interrupts, making it almost atomic  
    tst.w   _flock                              | unfortunately we can't use tas, as the _flock word is perused by different drivers differently - TOS sets 1.w, other only st the first byte etc.
    bne.s   castskipflock                       | don't do anything if flock is set - we can't use the DMA then, anyways
    
    move.w  #-1,_flock                          | keep others from using DMA  
    move.w  (SP)+,sr

    clr.w   (A1)                                | reset skip-counter for next frameskip
    jsr     (A0)                                | call worker

    move.w  (SP)+,_flock                        | set flock to previous state
castskipvbl:
    rts

castskipflock:
    move.w  (SP)+,sr
    move.w  (SP)+,_flock                        | set flock to previous state
    rts
    
cconws:
    pea       (A0)
    move.w    #9,-(SP)
    trap      #1
    addq.l    #6,SP
    rts

    .data
p1: .ascii "Screencast "
    dc.b 0
p2: .ascii "not "
    dc.b 0
p3: .ascii "installed. Press a key."
    dc.b 13,10,0

    .bss
__installed:            .ds.w   1
__vblcntscreen:         .ds.w   1
__vblskipscreen:        .ds.w   1
__vblcntconfig:         .ds.w   1
__vblskipconfig:        .ds.w   1
__savevbl:              .ds.l   1
__timerccount:          .ds.l   1
