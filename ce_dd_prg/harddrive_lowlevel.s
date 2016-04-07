
    .globl  _installHddLowLevelDriver
    .globl  _virtualDriveIndex

    .globl  _ceDrives
    .globl  _custom_getbpb
    .globl  _custom_mediach
    .globl  _myCRwabs
| ------------------------------------------------------	

_installHddLowLevelDriver:
    move.l  0x472,      oldGetBpb
    move.l  0x47e,      oldMediach
    move.l  0x476,      oldRwabs    
    
    move.l  #myAsmGetBpb,  0x472
    move.l  #myAsmMediach, 0x47e
    move.l  #myAsmRwabs,   0x476
    rts
    
| ------------------------------------------------------
myAsmGetBpb:
    movem.l d1,saveregs

    move.w  4(sp), d0           | d0 is drive number
    move.w  #1, d1
    lsl.w   d0, d1              | d1 = (1 << drive number) -- drive mask
    
    move.w  _ceDrives, d0       | get drives to d0
    and.w   d1, d0              | get only relevant drive bit
    
    movem.l saveregs, d1
    
    tst.w   d0                  | is the drive bit is 0 after applying drive mask? 
    beq     callOldGetBpb       | if this drive is not CE drive, use old GetBbp()

    lea     4(sp), a0           | get pointer to first param
    move.l  a0, -(sp)           | store it on stack - it will be the 1st param of the next C _function
    
    jsr     _custom_getbpb

    add.l   #4, sp              | restore stack addr
    rts

callOldGetBpb:
    move.l  oldGetBpb, a0
    jmp     (a0)
    
| ------------------------------------------------------
myAsmMediach:
    movem.l d1,saveregs

    move.w  4(sp), d0           | d0 is drive number
    move.w  #1, d1
    lsl.w   d0, d1              | d1 = (1 << drive number) -- drive mask
    
    move.w  _ceDrives, d0       | get drives to d0
    and.w   d1, d0              | get only relevant drive bit
    
    movem.l saveregs, d1
    
    tst.w   d0                  | is the drive bit is 0 after applying drive mask? 
    beq     callOldMediach      | if this drive is not CE drive, use old Mediach()

    lea     4(sp), a0           | get pointer to first param
    move.l  a0, -(sp)           | store it on stack - it will be the 1st param of the next C _function
    
    jsr     _custom_mediach

    add.l   #4, sp              | restore stack addr
    rts

callOldMediach:
    move.l  oldMediach, a0
    jmp     (a0)

| ------------------------------------------------------
myAsmRwabs:
    movem.l d1,saveregs

    move.w  14(sp), d0          | d0 is drive number
    move.w  #1, d1
    lsl.w   d0, d1              | d1 = (1 << drive number) -- drive mask
    
    move.w  _ceDrives, d0       | get drives to d0
    and.w   d1, d0              | get only relevant drive bit
    
    movem.l saveregs, d1
    
    tst.w   d0                  | is the drive bit is 0 after applying drive mask? 
    beq     callOldRwabs        | if this drive is not CE drive, use old Mediach()

    lea     4(sp), a0           | get pointer to first param
    move.l  a0, -(sp)           | store it on stack - it will be the 1st param of the next C _function
    
    jsr     _myCRwabs

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

_ceDrives:              .ds.w   1
_virtualDriveIndex:     .ds.w   1

saveregs:               .ds.l   16    

| ------------------------------------------------------	
