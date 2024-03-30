| functions to install VBL handler, uinstall VBL handler, and the actual VBL routine

    .globl  _install_vbl_routine
    .globl  _uninstall_vbl_routine
    .globl  _vblHandlerInC
    
    .text 

| -------------------------------------------------    

_install_vbl_routine:
    movem.l D0-A6,-(SP)

    move.l  0x70,__savevbl                      | save original VBL handler
    move.l  #screenirq,0x70                     | install VBL

    movem.l (SP)+,D0-A6
    rts

| -------------------------------------------------    

_uninstall_vbl_routine:
    movem.l D0-A6,-(SP)
    move.l  __savevbl, 0x70                     | restore original VBL
    movem.l (SP)+,D0-A6
    rts

| -------------------------------------------------    
screenirq:
    movem.l D0-A6,-(SP)

    lea     _vblHandlerInC,A0
    jsr     (A0)                                | call worker

    movem.l (SP)+,D0-A6
    move.l  __savevbl,-(SP)                     | call original VBL routine   
    rts
| -------------------------------------------------    

    .bss
__savevbl:              .ds.l   1

