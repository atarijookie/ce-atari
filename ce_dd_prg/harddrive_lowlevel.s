
    .globl  _installHddLowLevelDriver

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
    move.w  4(sp), d0           | d0 is drive number
    
    cmp.w   #2,d0               | if drive number is less than 2, it's a floppy, call original
    blo     callOldGetBpb
    
    movem.l d1,saveregs

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
    move.l  oldGetBpb, a0       | get old handler to A0
    
    move.l  a0,d0
    tst.l   d0                  | if the old handler is NULL, jump to noOldHandler (do nothing)
    beq     noOldHandler
    
    jmp     (a0)                | jump to old handler
    
| ------------------------------------------------------
myAsmMediach:
    move.w  4(sp), d0           | d0 is drive number
    
    cmp.w   #2,d0               | if drive number is less than 2, it's a floppy, call original
    blo     callOldMediach

    movem.l d1,saveregs
    
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
    move.l  oldMediach, a0      | get old handler to A0
    
    move.l  a0,d0
    tst.l   d0                  | if the old handler is NULL, jump to noOldHandler (do nothing)
    beq     noOldHandler
    
    jmp     (a0)                | jump to old handler

| ------------------------------------------------------
myAsmRwabs:
    move.w  14(sp), d0          | d0 is drive number
    
    cmp.w   #2,d0               | if drive number is less than 2, it's a floppy, call original
    blo     callOldRwabs
    
    movem.l d1,saveregs

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
    move.l  oldRwabs, a0        | get old handler to a0
    
    move.l  a0,d0
    tst.l   d0                  | if the old handler is NULL, jump to noOldHandler (do nothing)
    beq     noOldHandler
    
    jmp     (a0)                | jump to old handler

noOldHandler:
    rts
    
| ------------------------------------------------------	
	.bss

oldGetBpb:              .ds.l   1
oldMediach:             .ds.l   1
oldRwabs:               .ds.l   1

_ceDrives:              .ds.w   1

saveregs:               .ds.l   16    

| ------------------------------------------------------	
