|-------------------------------------------------

    .globl  _install_vbl
    .globl  _update_con_info

|-------------------------------------------------

    .text 

_flock      = 0x43e

_install_vbl:
    movem.l D0-A6,-(SP)

    move.l  0x70, __oldVbl
    move.l  #update_con_info_vbl,0x70           | install VBL   

    movem.l (SP)+,D0-A6
    rts

|-------------------------------------------------    
    
update_con_info_vbl:
    movem.l D0-A6,-(SP)             | back up registers

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
    
    jsr     _update_con_info        | call the update function
    |--------------
    
dontUpdateConInfo:
    
    movem.l (SP)+,D0-A6             | restore registers
    move.l  __oldVbl,-(SP)          | call original VBL routine   
    rts
    
|-------------------------------------------------    
    .bss
    
__vbl_counter:          .ds.w   1
__oldVbl:               .ds.l   1

