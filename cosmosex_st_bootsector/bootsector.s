	.text

	| write out title string
	pea		str(pc)
	move.w	#0x09,-(sp)
	trap	#1
	addq.l	#6,sp

	| malloc RAM, by default 70 kB, but is configurable
	lea		driverRam(pc),a0
	move.l	(a0), -(sp)			| read how much RAM we should allocate
	move.w	#0x48,-(sp)			| Malloc()
	trap	#1					|
	addq.l	#6,sp				|
	tst.l	d0
	beq		fail

	| store memory pointer to A1 and dma_pointer
	movea.l	d0,a1				| a1 is the DMA address
	lea		dma_pointer(pc),a0
	move.l	d0,(a0)

	| crate 0th cmd byte from ACSI ID (config + 2)
	lea		config(pc),a2		| load the config address to A2
	move.b	2(a2), d2			| d2 holds ACSI ID
	lsl.b	#5, d2				| d2 = d2 << 5
	ori.b	#0x08, d2			| d2 now contains (ACSI ID << 5) | 0x08, which is READ SECTOR from ACSI ID device

	| get the sector count from (config + 3)
	clr.l	d3
	move.b	3(a2), d3			| d3 holds sector count we should transfer
	subq.l	#1, d3				| d3--, because the dbra loop will be executed (d3 + 1) times

	moveq	#1, d1				| d1 holds the current sector number. Bootsector is 0, so starting from 1 to (sector count + 1)

	| trasnfer all the sectors from ACSI to RAM
readSectorsLoop:
	lea		acsiCmd(pc),a0		| load the address of ACSI command into a0
	move.b	d2,(a0)				| cmd[0] = ACSI ID + SCSI READ 6 command
	move.b	d1,3(a0)			| cmd[3] = sector number

	bsr		dma_read			| try to read the sector
	tst.b	d0
	bne.b	fail1				| d0 != 0?

	lea		512(a1),a1			| a1 += 512
	addq.w	#1,d1				| current_sector++
	dbra	d3, readSectorsLoop

|--------------------------------

	| now do the fixup for the loaded text position	

	movea.l	dma_pointer(pc),a0	| 
	lea		28(a0),a1			| A1 = points to text segment (prg header of size 0x1c was skipped) 
	move.l	a1,d5				| D5 = A1 (tbase)
	adda.l	2(a0),a1			| A1 += text size
	adda.l	6(a0),a1			| A1 += data size
	adda.l	14(a0),a1			| A1 += symbol size (most probably 0) == BYTE *fixups
								
	move.l	(a1)+, d1			| read fixupOffset, a1 now points to fixups array
	beq.s	skipFixing			| if fixupOffset is 0, then skip fixing
	
	add.l	d5, d1				| d1 = basePage->tbase + first fixup offset
	move.l	d1, a0				| a0 = fist fixup pointer
	moveq	#0, d0				| d0 = 0
	
fixupLoop:
	add.l	d5, (a0)			| a0 points to place which needs to be fixed, so add text base to that
getNextFixup:
	move.b	(a1)+, d0			| get next fixup to d0	
	beq.s	skipFixing			| if next fixup is 0, then we're finished
	
	cmp.b	#1, d0				| if the fixup is not #1, then skip to justFixNext
	bne.s	justFixupNext
	
	add.l	#0x0fe, a0			| if the fixup was #1, then we add 0x0fe to the address which needs to be fixed and see what is the next fixup
	bra.s	getNextFixup
	
justFixupNext:
	add.l	d0, a0				| new place which needs to be fixed = old place + fixup 
	bra.s	fixupLoop

| code will get here either after the fixup is done, or when the fixup was skipped	
skipFixing:
	
|--------------------------------

	| clear the BSS section of prg

	movea.l	dma_pointer(pc),a0	| sectors 1-N
	lea		28(a0),a1			| A1 = points to text segment (prg header of size 0x1c was skipped) 
	adda.l	2(a0),a1			| add text size
	adda.l	6(a0),a1			| add data size = bss segment starts here
	move.l	10(a0),d0			| d0.l = size of bss segment
	beq.b	bss_done
bss_loop:
	clr.b	(a1)+				
	subq.l	#1,d0
	bne.b	bss_loop
bss_done:

	move.l	#0x12345678,-(sp)	| store magic number for the startup code of driver to know that there's no basepage (meaning no base page)
	add.l	#4, sp
	
	lea		256(a0),a1			| A1 = points to text segment + some offset to the real start of the code
	jmp		(a1)				| jump to the code, but it won't return here
	
	|-----------------------------

	| if failed....
	
fail1:	
	move.l	dma_pointer(pc),-(sp)
	move.w	#0x49,-(sp)			| Mfree - free the allocate RAM
	trap	#1
	addq.l	#6,sp

fail:	
	pea		error(pc)			| CConws() - write out error
	move.w	#0x09,-(sp)
	trap	#1
	addq.l	#6,sp

	rts							| return back

	.data
	.even

dma_pointer:
	dc.l	0

| This is the configuration which will be replaced before sending the sector to ST.
| The format is : 'XX'  AcsiId  SectorCount driverRamBytes
| default values: 'XX'       0   32 (0x20)  70 kB (0x11800 bytes)

config:		.dc.l	0x58580020
driverRam:	.dc.l	0x00011800
acsiCmd:	.dc.b	0x08,0x00,0x00,0x00,0x01

str:	.ascii	"CEDD boot loader"
	.dc.b	13,10,0
error:	.ascii	"...failed :("
	.dc.b	13,10,0

	.text

gpip	= 0xFFFFFA01	| .B 68901 input register
dskctl	= 0xFFFF8604	| .W controller data access
fifo	= 0xFFFF8606	| .W DMA mode control
dmaHigh	= 0xFFFF8609	| .B DMA base high
dmaMid	= 0xFFFF860B	| .B DMA base mid
dmaLow	= 0xFFFF860D	| .B DMA base low
flock	= 0x43E			| .W DMA chiplock variable
dskbuf	= 0x4C6			| .L IK disk buffer
Hz_200	= 0x4BA			| .L 200 Hz counter
bootmg	= 0x1234		| .W boot checksum

| subroutine used to load single sector to memory from ACSI device
dma_read:
	lea	dskctl,	A5			| DMA data register
	lea	fifo,	A6			| DMA control register

	st	flock				| lock the DMA chip

	move.l	a1,-(SP)		| A1 contains the address to where the data should be transferred
	move.b	3(SP),dmaLow	| set up DMA pointer - low, mid, hi
	move.b	2(SP),dmaMid
	move.b	1(SP),dmaHigh
	addq	#4,sp			| return the SP to the original position

							| send cmd[0]
	move.w	#0x088,(A6)		| mode: NO_DMA | HDC;

	lea	(acsiCmd,pc),a0		| load the address of ACSI command into a0
	move.w	#4, d4			| loop count: 5 (dbra counts to this + 1)

sendCmdBytes:
	clr.l	d0				| d0 = 0
	move.w	#0x008a, d0		| d0 = 0000 008a
	swap	d0				| d0 = 008a 0000
	move.b	(a0)+, d0		| d0 = 008a 00 cmd[x]  -- d0 high = mode, d0 low = data
	bsr	pioWrite			| write cmd[x] and wait for IRQ
	tst.w	d0
	bne.s	dmr_fail		| if d0 != 0, error, exit on timeout
	dbra	d4, sendCmdBytes


	| toggle r/w, leave at read == clear FIFO
	move.w	#0x190,(A6)		| DMA_WR + NO_DMA + SC_REG
	move.w	#0x090,(A6)		|          NO_DMA + SC_REG

	move.w	#1,(A5)			| write sector count reg - will transfer 1 sector

	| send cmd[5]
	move.w	#0x008A, (a6)	| mode: NO_DMA + HDC + A0
	move.w	#0, (a5)		| data: 0
	move.w	#0, (a6)		| mode: start DMA transfer

	move.w	#200, d0		| 1s timeout limit
	bsr	waitForINT			|
	tst.w	d0
	bne.s	dmr_fail		| if d0 != 0, error, exit on timeout

	move.w	#0x08A,(A6)		| select status register
	move.w	(A5), D0		| get DMA return code
	and.w	#0x00FF, D0		| mask for error code only
	beq.s	dmr_success		| if D0 == 0, success!

dmr_fail:
	moveq	#-1,D0			| set error return (-1)

dmr_success:
	move.w	#0x080,(A6)		| reset DMA chip for driver
	sf	flock				| unlock DMA chip
	rts

| This routine writes single command byte in PIO mode and waits for ACSI INT or timeout.
pioWrite:
	move.w	d0, (a5)		| write data
	swap	d0
	move.w	d0, (a6)		| write mode

	moveq	#20, d0			| wait 0.1 second

waitForINT:
	add.l	Hz_200, d0		| set Dl to timeout

waitMore:
	btst.b	#5,gpip			| disk finished
	beq.s	gotINT			| OK return
	cmp.l	Hz_200, d0		| timeout yet?
	bne.s	waitMore		| no? try again

	moveq	#-1, d0			| d0 = fail!
	rts

gotINT:
	moveq	#0, d0			| d0 = success!
	rts
