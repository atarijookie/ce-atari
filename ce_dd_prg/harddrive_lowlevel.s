
    .globl  _installHddLowLevelDriver
    .globl  _virtualDriveIndex
    .globl  _virtualGetBpb
    .globl  _virtualRwabs

    .globl  _virtualHddEnabled
    .globl  _virtualDriveChanged
    .globl  _virtualBpbPointer
    
| ------------------------------------------------------	

_installHddLowLevelDriver:
	movem.l d1/d2, saveregs	    | save registers
    
    move.l  0x472,      oldGetBpb
    move.l  0x47e,      oldMediach
    move.l  0x476,      oldRwabs    
    
    move.l  #myGetBpb,  0x472
    move.l  #myMediach, 0x47e
    move.l  #myRwabs,   0x476

    move.w  _virtualDriveIndex, d1  | get our drive index
    move.l  #1,d2
    rol.l   d1,d2               | D2 is now bitmask representing our drive
    
    move.l  0x4c2, d0           | read DRVBITS
    or.w    d2, d0              | add drive 'D'
    move.l  d0, 0x4c2           | put it back

	movem.l saveregs, d1/d2     | restore registers
    
    rts
    
| ------------------------------------------------------
myGetBpb:
    move.w  _virtualHddEnabled, d0
    tst.w   d0
    beq     callOldGetBpb               | if virtual hdd not enabled, jump to old

    move.w  _virtualDriveIndex, d0
    cmp.w   4(sp), d0
    bne     callOldGetBpb               | if requested drive is not ours, jump to old
    
    move.l  _virtualBpbPointer, d0      | read the pointer to D0 and quit
    rts

callOldGetBpb:
    move.l  oldGetBpb, a0
    jmp     (a0)
    
| ------------------------------------------------------
myMediach:
    move.w  _virtualHddEnabled, d0
    tst.w   d0
    beq     callOldGetBpb               | if virtual hdd not enabled, jump to old

    move.w  _virtualDriveIndex, d0
    cmp.w   4(sp), d0
    bne     callOldMediach              | if requested drive is not ours, jump to old
    
    clr.l   d0
    move.w  _virtualDriveChanged, d0    | return the current status of drive changed
    move.w  #0,_virtualDriveChanged     | not changed in the next call
    rts

callOldMediach:
    move.l  oldMediach, a0
    jmp     (a0)

| ------------------------------------------------------
myRwabs:
    move.w  _virtualHddEnabled, d0
    tst.w   d0
    beq     callOldGetBpb       | if virtual hdd not enabled, jump to old

    move.w  _virtualDriveIndex, d0
    cmp.w   14(sp), d0
    bne     callOldRwabs        | if requested drive is not ours, jump to old
    
    lea     4(sp), a0           | get pointer to first param
    move.l  a0, -(sp)           | store it on stack - it will be the 1st param of the next C _function
    
    jsr     _myRwabs            | it's our drive, call our Rwabs()

    add.l   #4, sp              | restore stack addr
    rts

callOldRwabs:
    move.l  oldRwabs, a0
    jmp     (a0)

| ------------------------------------------------------	
	.bss

oldGetBpb:              .ds.l   1
oldMediach:             .ds.l   1
oldRwabs:               .ds.l   1

_virtualHddEnabled:     .ds.w   1
_virtualDriveIndex:     .ds.w   1
_virtualDriveChanged:   .ds.w   1
_virtualBpbPointer:     .ds.l   1
saveregs:               .ds.l   16    

| ------------------------------------------------------	
