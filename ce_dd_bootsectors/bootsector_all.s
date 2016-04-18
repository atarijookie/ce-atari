    .text

    | write out title string
    pea     str(pc)
    move.w  #0x09,-(sp)
    trap    #1
    addq.l  #6,sp

    | malloc RAM, by default 70 kB, but is configurable
    lea     driverRam(pc),a0
    move.l  (a0), -(sp)         | read how much RAM we should allocate
    move.w  #0x48,-(sp)         | Malloc()
    trap    #1                  |
    addq.l  #6,sp               |
    tst.l   d0
    beq     fail

    | store memory pointer to A1 and dma_pointer
    movea.l d0,a1               | a1 is the DMA address
    lea     config(pc),a2       | load the config address to A2
    lea     dma_pointer(pc),a0
    move.l  d0,(a0)

	cmp.w 	#0x200,0x2.w 		|XBIOS 42 (DMAread) is present if >=TOS 2.0
	blt.s 	noxbios42

	|----------------------------------
	|first try DMAread
	movem.l D1-A6,-(SP)
	|int16_t DMAread( int32_t sector, int16_t count, void *buffer, int16_t devno );
	moveq 	#0,D0
	move.b  (A2),D0
	move.w 	D0,-(sp)  	| devno 	Offset 12
	pea     (A1)       	| buffer 	Offset  8
    move.b  1(a2),D0    | d0 holds sector count we should transfer
	move.w  D0,-(sp)  	| count 	Offset  6
	move.l  #0,-(sp) 	| Offset  2
	move.w  #42,-(sp)   | Offset  0
	trap    #14         | Call XBIOS
	lea     14(sp),sp   | Correct stack
	movem.l (SP)+,D1-A6
	tst.l 	D0    		|0=OK, <0=TOS Error, (>0=not supported ?)
	beq.s	dmareadok	|read OK?
	blt.s	fail1 		|read error

	|----------------------------------
noxbios42:
	|only try ST dma if this isn't a Falcon or TT
	move.l 	0x5a0.w,D0
 	beq.s	is_st 		|no Cookie Jar? This is an ST!
 	move.l 	D0,A3
cookieloop:
	move.l 	(A0)+,D0
 	beq.s	is_st          |no _MCH Cokie found? This is an ST!
 	move.l 	(A0)+,D1
 	cmp.l 	#0x5F4D4348,D0 |_MCH
 	bne.s 	cookieloop
|Cookie Value		Description
|_MCH	$0000xxxx	STf
|_MCH	$00010000	STe
|_MCH	$00010010	Mega-STe ( only bit 4 of the 32bits value tells it's a Mega-STE )
|_MCH	$0002xxxx	TT
|_MCH	$0003xxxx	Falcon
|CT60	$xxxxxxxx	CT60
	|do we have a TT, F030 or above? Something else went wrong, DMAread should have succeeded
	cmp.l   #0x20000,D1
	bge.s  	fail1	
	|we are an ACSI-class System, let's try a manual DMA read	 	
is_st: 
    | crate 0th cmd byte from ACSI ID (config + 2)
    move.b  (a2), d2            | d2 holds ACSI ID
    lsl.b   #5, d2              | d2 = d2 << 5
    ori.b   #0x08, d2           | d2 now contains (ACSI ID << 5) | 0x08, which is READ SECTOR from ACSI ID device

    | get the sector count from (config + 3)
    clr.l   d3
    move.b  1(a2), d3           | d3 holds sector count we should transfer

    | transfer all the sectors from ACSI to RAM
readSectorsLoop:
    lea     acsiCmd(pc),a0      | load the address of ACSI command into a0
    move.b  d2,(a0)             | cmd[0] = ACSI ID + SCSI READ 6 command
    move.b  d3,4(a0)            | cmd[4] = sector sount we should transfer

    bsr     dma_read            | try to read the sector
    tst.b   d0
    bne.b   readSectorsLoop     | if failed to read sector, try reading sector again
dmareadok:
|--------------------------------
| driver is loaded in RAM, now to do the rest

    lea     dma_pointer(pc),a0
    move.l  (a0), a1            | restore pointer to allocated RAM in A1

    move.l  a1,   a2            | A2 points to Level 2 bootsector
    add.l   #512, a2            | A2 now points to driver (512 bytes after L2 bootsector)
    
    jmp     (a1)                | jump to Level 2 bootsector, which will do fixup and clearing BSS
    
    |-----------------------------

    | if failed....
    
fail1:  
    move.l  dma_pointer(pc),-(sp)
    move.w  #0x49,-(sp)         | Mfree - free the allocate RAM
    trap    #1
    addq.l  #6,sp

fail:   
    pea     error(pc)           | CConws() - write out error
    move.w  #0x09,-(sp)
    trap    #1
    addq.l  #6,sp

    rts                         | return back

    .data
    .even
							
dma_pointer:
    dc.l    0

| This is the configuration which will be replaced before sending the sector to ST.
| The format is : 'XX'  driverRamBytes          AcsiId     SectorCount 
| default values: 'XX'  70 kB (0x11800 bytes)      0        32 (0x20)  

configTag:  .dc.w   0x5858
driverRam:  .dc.l   0x00011800
config:     .dc.b   0x00,0x20
acsiCmd:    .dc.b   0x08,0x00,0x00,0x01,0x20

str:    .ascii  "\n\rCE boot..."
        .dc.b   13,10,0
error:  .ascii  "fail"
        .dc.b   13,10,0

    .text
    .even

gpip    = 0xFFFFFA01    | .B 68901 input register
dskctl  = 0xFFFF8604    | .W controller data access
fifo    = 0xFFFF8606    | .W DMA mode control
dmaHigh = 0xFFFF8609    | .B DMA base high
dmaMid  = 0xFFFF860B    | .B DMA base mid
dmaLow  = 0xFFFF860D    | .B DMA base low
flock   = 0x43E         | .W DMA chiplock variable
dskbuf  = 0x4C6         | .L IK disk buffer
Hz_200  = 0x4BA         | .L 200 Hz counter
bootmg  = 0x1234        | .W boot checksum

| subroutine used to load single sector to memory from ACSI device
dma_read:
    lea dskctl, A5          | DMA data register
    lea fifo,   A6          | DMA control register

    st  flock               | lock the DMA chip

    move.l  a1,-(SP)        | A1 contains the address to where the data should be transferred
    move.b  3(SP),dmaLow.w  | set up DMA pointer - low, mid, hi
    move.b  2(SP),dmaMid.w
    move.b  1(SP),dmaHigh.w
    addq    #4,sp           | return the SP to the original position

                            | send cmd[0]
    move.w  #0x088,(A6)     | mode: NO_DMA | HDC;

    lea (acsiCmd,pc),a0     | load the address of ACSI command into a0
    move.w  #4, d4          | loop count: 5 (dbra counts to this + 1)

sendCmdBytes:
    clr.l   d0              | d0 = 0
    move.w  #0x008a, d0     | d0 = 0000 008a
    swap    d0              | d0 = 008a 0000
    move.b  (a0)+, d0       | d0 = 008a 00 cmd[x]  -- d0 high = mode, d0 low = data
    bsr pioWrite            | write cmd[x] and wait for IRQ
    tst.w   d0
    bne.s   dmr_fail        | if d0 != 0, error, exit on timeout
    dbra    d4, sendCmdBytes

    | toggle r/w, leave at read == clear FIFO
    move.w  #0x190,(A6)     | DMA_WR + NO_DMA + SC_REG
    move.w  #0x090,(A6)     |          NO_DMA + SC_REG

    lea     (acsiCmd,pc),a0 | load the address of ACSI command into a0
    clr.l   d0
    move.b  4(a0), d0       | retrieve sector count to D0 register
    move.w  d0,(A5)         | write sector count reg - will transfer THIS MANY sector

    | send cmd[5]
    move.w  #0x008A, (a6)   | mode: NO_DMA + HDC + A0
    move.w  #0, (a5)        | data: 0
    move.w  #0x0A, (a6)     | mode: HDC + A0 start DMA transfer

    move.w  #200, d0        | 1s timeout limit
    bsr waitForINT          |
    tst.w   d0
    bne.s   dmr_fail        | if d0 != 0, error, exit on timeout

    move.w  #0x08A,(A6)     | select status register
    move.w  (A5), D0        | get DMA return code
    and.w   #0x00FF, D0     | mask for error code only
    beq.s   dmr_success     | if D0 == 0, success!

dmr_fail:
    moveq   #-1,D0          | set error return (-1)

dmr_success:
    move.w  #0x080,(A6)     | reset DMA chip for driver
    sf  flock               | unlock DMA chip
    rts

| This routine writes single command byte in PIO mode and waits for ACSI INT or timeout.
pioWrite:
    move.w  d0, (a5)        | write data
    swap    d0
    move.w  d0, (a6)        | write mode

    moveq   #20, d0         | wait 0.1 second

waitForINT:
    add.l   Hz_200.w, d0      | set Dl to timeout

waitMore:
    btst.b  #5,gpip         | disk finished
    beq.s   gotINT          | OK return
    cmp.l   Hz_200.w, d0      | timeout yet?
    bne.s   waitMore        | no? try again

    moveq   #-1, d0         | d0 = fail!
    rts

gotINT:
    moveq   #0, d0          | d0 = success!
    rts
    
