
.equ gpip,		0xFFFFFA01	| .B 68901 input register
.equ dskctl,	0xFFFF8604	| .W controller data access
.equ fifo,		0xFFFF8606	| .W DMA mode control
.equ dmaHigh,	0xFFFF8609	| .B DMA base high
.equ dmaMid,	0xFFFF860B	| .B DMA base mid
.equ dmaLow,	0xFFFF860D	| .B DMA base low
.equ flock,		0x43E		| .W DMA chiplock variable
.equ dskbuf,	0x4C6		| .L IK disk buffer
.equ Hz_200,	0x4BA		| .L 200 Hz counter
.equ bootmg,	0x1234		| .W boot checksum
	
| ------------------------------------------------------	
	.text
| ------------------------------------------------------
|	move.l	#0xfffffffe, a0
|	move.l	#1, (a0)

	movem.l	d1-d7/a0-a6,-(sp)	| save registers content

| the real stuff starts here:
	
| this part does Malloc()
	move.l	#60000, d1			| size of malloc - CE driver needs 55080 bytes in RAM now
	move.l	d1,-(sp)			| malloc those bytes
	move.w	#0x48,-(sp)
	trap	#1
	addq.l	#6,sp

	tst.l	d0					| did malloc fail?
	beq		end_fail			| malloc failed, jump to end

	lea		(pRam,pc),a2		| load the malloc address storage pointer to A2
	move.l	d0, (a2)			| store the original address from malloc to pRam
	
	add.l	#2, d0				| d0 += 2
	andi.l	#0xfffffffe, d0		| d0 is now EVEN

	move.l	d0, a3				| A3 = store the memory pointer where the driver will be loaded
	move.l	d0, a1				| A1 will hold the current DMA transfer address, and it will move further by sector size
	move.l	d0, a4				| A4 = this is hopefully EVEN address where the driver will be loaded

| loading of the driver from ACSI
	lea		(config,pc),a2		| load the config address to A2
	move.b	2(a2), d2			| d2 holds ACSI ID 
	lsl.b	#5, d2				| d2 = d2 << 5
	ori.b	#0x08, d2			| d2 now contains (ACSI ID << 5) | 0x08, which is READ SECTOR from ACSI ID device
	
	clr.l	d3
	move.b	3(a2), d3			| d3 holds sector count we should transfer
	sub.l	#1, d3				| d3--, because the dbra loop will be executed (d3 + 1) times
	
	move.l	#1, d1				| d1 holds the current sector number. Bootsector is 0, so starting from 1 to (sector count + 1)
	
readSectorsLoop:	 
	lea		(acsiCmd,pc),a0		| load the address of ACSI command into a0
	move.b	d2, (a0)			| cmd[0] = ACSI ID + SCSI READ 6 command
	move.b	d1, 3(a0)			| cmd[3] = sector number
	
	bsr		dma_read			| try to read the sector
	tst.b	d0					
	bne		free_end_fail		| d0 != 0?
	
	add.l	#512, a1			| a1 += 512
	add.w	#1, d1				| current_sector++
	dbra	d3, readSectorsLoop
	
| at this point the driver should be loaded at the location pointed by pDriver

|--------------------------------
| first fill the correct into in the Base Page
	move.l	a4, a0				| read the value stored at pDriver
	add.l	#2, a0				| a0 now points to prgHead->tsize

	move.l	a4, d5
	add.l	#0x1c, d5			| d5 = pointer to text segment, save it for the fixup loop usage
	
	move.l	(a0)+, d1			| d1 = prgHead->tsize, a0 moved to prgHead->dsize
	move.l	(a0)+, d0			| d0 = prgHead->dsize, a0 moved to prgHead->bsize
	add.l	d0, d1				| d1 = prgHead->tsize + prgHead->dsize
	move.l	d1, d0				| d0 = tsize + dsize
	add.l	d5, d0				| d0 = text segment pointer + tsize + dsize = pointer to bss
	move.l	d0, a2				| a2 = pointer to bss, save it for clearing BSS section
	move.l	(a0)+, d6			| d6 = prgHead->bsize, a0 moved to prgHead->ssize, save it for clearing BSS section
	move.l	(a0), d0			| d0 = prgHead->ssize
	add.l	d0, d1				| d1 = prgHead->tsize + prgHead->dsize + prgHead->ssize
	
	move.l	a4, d0				| d0 points to the start of driver file
	add.l	#0x1c, d0			| d0 now points to start of text
	add.l	d1, d0				| d0 now points to fixup offset (start + text offset + prgHead->tsize + prgHead->dsize + prgHead->ssize) == BYTE *fixups
	
|--------------------------------
| now do the fixup for the loaded text position	

	move.l	d0, a1								
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
| now we need to clear the BSS section, because it's filled either with symbols, or fixups
	| a2 = basePage->bbase
	| d6 = bsize	
	
	tst.l	d6
	beq.s	skipMemsetBss

	sub.l	#1, d6				| d6--, because dbra will be executed (d6 + 1) times
	move.l	#0, d0				| d0 = 0
	
memsetLoop:	
	move.b	d0, (a2)+			| *A2 = 0; A2++;
	dbra	d6, memsetLoop
	
skipMemsetBss:
	
	
| Finally execute the code...
	move.l	#0, -(sp)			| this will be 4(sp), and 0 will mean that we're running from boot sector
	move.l	#0, -(sp)			| this is here just to create the offset for the previous line

	move.l	a3, d0				| d0 = pointer to start of driver file, memory received from malloc()
	add.l	#0x100, d0			| d0 += 256, points to start of text
	move.l	d0, a0
	
	jmp		(a0)				| jump to the driver code... this should install the code
	
	add.l	#8, sp				| fix the SP after those 2 move.l #0, -(sp) up there

	bra		end_good			| now finish with a good result
|--------------------------------------------------------------------------------------------------------------------	

	
| jump here to free the memory and finish on fail	
free_end_fail:

	lea		(pRam,pc),a2		| load the malloc address storage pointer to A2
	move.l	(a2), d0			| get the original address from malloc to pRam

	move.l	d0, -(sp)
	move.w	#0x49, -(sp)		| Mfree() on the memory from Malloc() 
	trap	#1
	addq	#6, sp

| jump here to finish on fail without freeing the memory
end_fail:
end_good:	

	movem.l	(sp)+, d1-d7/a0-a6	| restore register content
	rts							| return to calling code
	
|--------------------------------------------------------------------------------------------------------------------	
| subroutine used to load single sector to memory from ACSI device
dma_read:
	lea		dskctl,	A5		| DMA data register	
	lea		fifo,	A6		| DMA control register	
	
	st		flock			| lock the DMA chip
	
	move.l	a1,-(SP)		| A1 contains the address to where the data should be transferred
	move.b	3(SP),dmaLow	| set up DMA pointer - low, mid, hi
	move.b	2(SP),dmaMid		
	move.b	1(SP),dmaHigh		
	addq	#4,sp			| return the SP to the original position
	
	| send cmd[0]
	move.w	#0x088,(A6)		| mode: NO_DMA | HDC;

	lea		(acsiCmd,pc),a0	| load the address of ACSI command into a0
	move.w	#4, d4			| loop count: 5 (dbra counts to this + 1)
	
sendCmdBytes:	
	clr.l	d0				| d0 = 0
	move.w	#0x008a, d0		| d0 = 0000 008a
	swap	d0				| d0 = 008a 0000
	move.b	(a0)+, d0		| d0 = 008a 00 cmd[x]  -- d0 high = mode, d0 low = data
	bsr		pioWrite		| write cmd[x] and wait for IRQ
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
	bsr		waitForINT		|
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
	sf		flock			| unlock DMA chip
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

| ------------------------------------------------------
	.data
| This is the configuration which will be replaced before sending the sector to ST. 
| The format is: 'XX'  AcsiId  SectorCount 
config:		dc.l			0x58580020			
pRam:		dc.l			0

acsiCmd:	dc.b			0x08, 0x00, 0x00, 0x00, 0x01					
| ------------------------------------------------------	
