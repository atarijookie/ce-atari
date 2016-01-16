| ------------------------------------------------------
|
| This code came from:
| https://github.com/ggnkua/Atari_ST_Sources/blob/master/ASM/Various/FDC_ROUT.S
|
| ------------------------------------------------------
| Routines for handling FDC directly, not through Floprd() TOS function. 

    .globl  _argFuncId
    .globl  _argDrive
    .globl  _argTrack
    .globl  _argSide
    .globl  _argSector
    .globl  _argLeaveOn
    .globl  _argBufferPtr
    .globl  _argSuccess
    .globl  _fdcStatus
    
    .globl  _runFdcAsm

    .globl  _seekRate
| ------------------------------------------------------
restore     = 0   | reset drive to track zero
seek        = 1   | goto track
step        = 2   | move drive in last Direction
stepin      = 3   | track=track+1
stepout     = 4   | track=track-1
read        = 5   | read sector
write       = 6   | write sector
readid      = 7   | get ID fields ( NOT DONE )
readtrk     = 8   | read track
writetrk    = 9   | 

| ------------------------------------------------------
    .text

_runFdcAsm:    
    movem.l d1-d7/a0-a6,-(sp)

    move.l  _argSector,d0       | sector
    move.l  _argTrack,d1        | track
    move.l  _argSide,d2         | side
    move.l  _argDrive,d3        | drive
    move.l  _argLeaveOn,d4      | leave drive on
    move.l  _argFuncId,d7       | function number
    move.l  _argBufferPtr,a0    | pointer to buffer where the data will go

    bsr     RunFDC
    move.l  d7, _argSuccess | copy return code from D7 to global variable
    
    movem.l (sp)+,d1-d7/a0-a6
    rts
    
| ------------------------------------------------------
|  d0=sector
|  d1=track
|  d2=side
|  d3=drive
|  d4=turn drive off after operation (0=turn it off 1=leave it on)
|  d7=function number (replaced with 0(no error) or -1(timeout) )
|  a0=where to write/read data to/from

RunFDC:
    move    #1,0x43E.w          | lock dma
    movem.l d0-d6/a0-a6,-(sp)
    lea     0xFFFF8604.w,a1     | data reg
    lea     0xFFFF8606.w,a2     | command reg
    lea     0xFFFF8609.w,a3     | address reg
    lea     0xFFFFFA01.w,a4     | mfp gpip
    lea     0xFFFF8800.w,a5     | sound chip

    bsr     Select_Drive

    lsl.l   #2,d7               | function number times 4
    move.l  functiontable(pc,d7),a6
    jsr     (a6)

    move    #0x180,(a2)         | select status reg
    move.w  (a1),_fdcStatus     | read & store status

    tst     d4
    bne     notoff

    bsr     MotorOff
    move    #-1,d3
    bsr     Select_Drive

notoff:
    movem.l (sp)+,d0-d6/a0-a6
    move    #0,0x43E.w          | unlock
    rts

functiontable:
    dc.l    Restore             | 0 - reset FDC seek track zero
    dc.l    Seek                | 1 - goto previous track
    dc.l    Step                | 2 - move drive in last Direction
    dc.l    StepIn              | 3 - track=track+1
    dc.l    StepOut             | 4 - track=track-1
    dc.l    Read                | 5 - read sector
    dc.l    Write               | 6 - write sector
    dc.l    ReadID              | 7 - get ID fields ( NOT DONE )
    dc.l    ReadTrk             | 8 - read track
    dc.l    WriteTrk            | 9  

| ------------------------------------------------------
|  d6 id used by wfeoc to signal a time out
|  -1 = time out  0 = ok
| ------------------------------------------------------
Restore:
    clr.l   d6
    move.b  _seekRate,d6        | get seek rate
    or.b    #0,d6               | add command RESTORE (0) to seek rate (I know this is useless, this is just to make this more readable)
    
    move    #0x80,(a2)          | select command register
    move    d6,(a1)             | do restore - with desired seek rate
    bsr     wfeoc               | wait for end of command
    move.l  d6,d7
    rts
| ------------------------------------------------------
Seek:
    move    #0x86,(a2)          | select data register
    move    d1,(a1)             | write track to it
    move    (a2),stat
    
    clr.l   d6
    move.b  _seekRate,d6        | get seek rate
    or.b    #0x10,d6            | add command SEEK (0x10) to seek rate
    
    move    #0x80,(a2)          | select command register
    move    d6,(a1)             | perform seek - with desired seek rate
    bsr     wfeoc               | wait for end of command
    move.l  d6,d7
    rts
| ------------------------------------------------------
Step:   
    move    #0x80,(a2)          | select command register
    move    #0x31,(a1)          | perform step
    bsr     wfeoc               | wait for end of command
    move.l  d6,d7
    rts
| ------------------------------------------------------
StepIn:
    move    #0x80,(a2)          | select command register
    move    #0x51,(a1)          | perform step in
    bsr     wfeoc               | wait for end of command
    move.l  d6,d7
    rts
| ------------------------------------------------------
StepOut:
    move    #0x80,(a2)          | select command register
    move    #0x71,(a1)          | perform step out
    bsr     wfeoc               | wait for end of command
    move.l  d6,d7
    rts
| ------------------------------------------------------
Read:
    move    #0x84,(a2)          | select sector register
    move    d0,(a1)             | set sector

    pea     (a0)
    move.b  3(sp),4(a3)         | lo byte first
    move.b  2(sp),2(a3)         | middle byte secound
    move.b  1(sp),0(a3)         | hi byte last
    addq    #4,sp

    move    #0x90,(a2)          | set dma to read status
    move    #0x190,(a2)
    move    #0x90,(a2)
    move    #1,(a1)             | read one sector

    move    #0x80,(a2)          | select command register
    move    #0x80,(a1)          | read one sector

    bsr     wfeoc               | wait for end of command
    move.l  d6,d7
    rts

| ------------------------------------------------------
Write:
    move    #0x184,(a2)         | select sector register
    move    d0,(a1)             | set sector

    pea     (a0)
    move.b  3(sp),4(a3)         | lo byte first
    move.b  2(sp),2(a3)         | middle byte secound
    move.b  1(sp),0(a3)         | hi byte last
    addq    #4,sp

    move    #0x190,(a2)         | set dma to write status
    move    #0x90,(a2)
    move    #0x190,(a2)
    move    #1,(a1)             | write one sector

    move    #0x180,(a2)         | select command register
    move    #0xa0,(a1)          | write one sector

    bsr     wfeoc               | wait for end of command
    move.l  d6,d7
    rts
| ------------------------------------------------------
ReadID:
    rts

| ------------------------------------------------------
ReadTrk:
    pea     (a0)
    move.b  3(sp),4(a3)         | lo byte first
    move.b  2(sp),2(a3)         | middle byte secound
    move.b  1(sp),0(a3)         | hi byte last
    addq    #4,sp

    move    #0x90,(a2)          | set dma to read status
    move    #0x190,(a2)
    move    #0x90,(a2)
    move    #13,(a1)            | read 512*13 bytes

    move    #0x80,(a2)          | select command register
    move    #0xE0,(a1)          | read track

    bsr     wfeoc               | wait for end of command
    move.l  d6,d7

    rts
| ------------------------------------------------------
WriteTrk:
    pea     (a0)
    move.b  3(sp),4(a3)         | lo byte first
    move.b  2(sp),2(a3)         | middle byte secound
    move.b  1(sp),0(a3)         | hi byte last
    addq    #4,sp

    move    #0x190,(a2)         | set dma to write status
    move    #0x90,(a2)
    move    #0x190,(a2)
    move    #13,(a1)            | write 512*13 bytes

    move    #0x180,(a2)         | select command register
    move    #0xF0,(a1)          | read track

    bsr     wfeoc               | wait for end of command
    move.l  d6,d7

    rts

| ------------------------------------------------------
|   RunFDC subroutines (called internaly by the functions)
| ------------------------------------------------------
wfeoc:
    move.l  #2000000,d6         | gives approx. 4 secounds

.wfeoc:
    subq.l  #1,d6
    beq     Timed_out
    nop
    nop
    btst    #5,(a4)
    bne.s   .wfeoc
    clr.l   d6
    rts
    
Timed_out:
    moveq   #-1,d6
    rts
| ------------------------------------------------------
Select_Drive:
    eor     #1,d2               | flip side
    tst     d3
    bmi     Nodrive
    beq     DriveA
    move    #0b0010,d3
    bra     Setd
DriveA:
    move    #0b0100,d3
    bra     Setd
Nodrive:
    move    #0b0110,d3
Setd:
    or      d2,d3
    move.b  #14,(a5)
    move.b  d3,2(a5)
    rts
| ------------------------------------------------------
MotorOff:
    move    #0x180,(a2)         | select status reg
motson:
    move    (a1),d4
    tst.b   d4
    bmi.s   motson
    move    d4,stat
    rts
| ------------------------------------------------------

| ------------------------------------------------------
    .bss
| ------------------------------------------------------
stat:       .ds.w   1
readbuff:   .ds.b   15000        | for read track, was 6656

| The following DWORDs are used for argument transfer between C code and asm code.
| I know, I could write code for reading from stack and putting it to regs, but as
| I'm a lazy mofo, I did it this way ;)
_argFuncId:     .ds.l   1
_argDrive:      .ds.l   1
_argTrack:      .ds.l   1
_argSide:       .ds.l   1
_argSector:     .ds.l   1
_argLeaveOn:    .ds.l   1
_argBufferPtr:  .ds.l   1
_argSuccess:    .ds.l   1
