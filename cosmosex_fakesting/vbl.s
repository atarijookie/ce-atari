|-------------------------------------------------

    .globl  _install_vbl
    .globl  _update_con_info
    .globl  _update_con_info_vbl
    .globl  _fromVbl
    .globl  _vblEnabled
    
|-------------------------------------------------

    .text 

_flock      = 0x43e

_install_vbl:
    movem.l D0-A6,-(SP)

    move.l  0x70, __oldVbl
    move.l  #_update_con_info_vbl,0x70      | install VBL   

    movem.l (SP)+,D0-A6
    rts

|-------------------------------------------------    
    
_update_con_info_vbl:
    #move.w  sr,-(SP)                | save Status Register
    #move    #0x2700,sr              | protect this from interrupts, making it almost atomic

    movem.l D0-A6,-(SP)             | back up registers

    tst.w   _vblEnabled             | read this enabled flag
    beq     dontUpdateConInfo       | if vblEnabled is 0, then skip our vbl routine
    
    lea     __vbl_counter, a0       | get address of conter variable
    move.w  (a0), d0                | get value
    add.w   #1, d0                  | increment it
    move.w  d0, (a0)                | store it again
    
    cmp.w   #25, d0
    blt     dontUpdateConInfo       | if less than 25 ints happened, dont update con info

    |--------------
    | we counted enough VBLs
    move.w  #0, (a0)                | clear the counter
    
    tst.w   _flock                  | is flock set? if so, don't update
    bne.s   dontUpdateConInfo       | don't do anything if flock is set - we can't use the DMA then, anyways
    
    move.w  #1, _fromVbl            | set flag: we're in VBL
    move.l  #0, -(sp)               | forceUpdate = 0
    
    jsr     _update_con_info        | call the update function
    
    add.l   #4, sp
    move.w  #0, _fromVbl            | clear flag: we're not in VBL
    |--------------
    
dontUpdateConInfo:
    
    movem.l (SP)+,D0-A6             | restore registers
    #move.w  (SP)+,sr                | restore Status Register
    
    move.l  __oldVbl,-(SP)          | call original VBL routine   
    rts
    
|-------------------------------------------------    
    .bss
    
__vbl_counter:          .ds.w   1
__oldVbl:               .ds.l   1
_fromVbl:               .ds.w   1
_vblEnabled:            .ds.w   1
