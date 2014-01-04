
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
|.equ PRG,		1
|.equ FROMFILE,	1
|.equ FAKEJSR,	1

|	move.l	#0xfffffffe, a0
|	move.l	#1, (a0)

.ifdef PRG
	| the following will not be needed in the real boot sector
	| mshrink()
	move.l	4(sp),a5				| address to basepage

	| move the stack to end of our ram after mshrink()
	move.l	a5,d0					| d0 = address of BP
	add.l	#10000, d0				| d0 = end of our RAM
	and.b	#0xf0,d0				| align stack
	move.l	d0,sp					| new stackspace

	| do the mshrink()
	move.l	#10000, -(sp)			| memory size to keep 
	move.l	a5,-(sp)				|
	clr.w	-(sp)					|
	move.w	#0x4a,-(sp)				|
	trap	#1						|	
	add.l	#12,sp					
.endif	

.ifdef FAKEJSR
	move.l	#fakeSubRoutine, a0
	jsr		(a0)
	
	clr.w	-(sp)	| terminate
	trap	#1

fakeSubRoutine:
|	nop
|	rts
	
.endif


	movem.l	d1-d7/a0-a6,-(sp)	| save registers content

.ifdef PRG
	| the following will not be needed in the real boot sector
	| go to supervisor mode
	move.l #0, -(sp)
	move.w #0x20,-(sp)
	trap #1
	addq.l #6,sp
	
	move.l	d0, pStack
.endif
					
 
| the real stuff starts here:

	| Pexec: create basepage and allocate RAM
	moveq	#0, d1				| d1 = 0
	
	move.l	d1, -(sp)			| envstr  = 0
	move.l	d1, -(sp)			| cmdline = 0
	move.l	d1, -(sp)			| fname   = 0
	move.w	#5, -(sp)			| mode    = PE_BASEPAGE
	move.w	#0x4b, -(sp)		| Pexec()
	trap	#1
	add.l	#16, sp

	cmp.l	d1, d0				| if Pexec failed (do <= 0), jump to end
	ble		end_fail

	move.l	d0, a3				| A3 = store the original memory pointer, hoping that if will be an EVEN address
	add.l	#(256 - 28), d0		| store the driver at this position after start of basepage
	move.l	d0, a1				| A1 will hold the current DMA transfer address, and it will move further by sector size
	move.l	d0, a4				| A4 = this is hopefully EVEN address where the driver will be loaded

.ifndef FROMFILE	
	lea		(config,pc),a2		| load the config address to A2
	move.b	2(a2), d2			| d2 holds ACSI ID 
	lsl.b	#5, d2				| d2 = d2 << 5
	ori.b	#0x08, d2			| d2 now contains (ACSI ID << 5) | 0x08, which is READ SECTOR from ACSI ID device
	
	clr.l	d3
	move.b	3(a2), d3			| d3 holds sector count we should transfer
	sub.l	#1, d3				| d3--, because the dbra loop will be executed (d3 + 1) times
	 
	move.l	#1, d1				| d1 holds the current sector number. Bootsector is 0, so starting from 1 to (sector count + 1)
	 
readSectorsLoop:	 
	bsr		dma_read			| try to read the sector
	tst.b	d0					
	bne		free_end_fail		| d0 != 0?
	
	add.l	#512, a1			| a1 += 512
	add.w	#1, d1				| current_sector++
	dbra	d3, readSectorsLoop
.else 
	| if - for testing purposes - we should load it from file instead of ACSI device
	
	| fopen
	move.w	#0,-(sp)
	move.l	#fname, -(sp)
	move.w	#0x3D,-(sp)
	trap 	#1
	addq.l	#8,sp
	
	move.l	d0, handle

	| fread
	move.l	a1, -(sp)			| buffer
	move.l	#10000, -(sp)		| size
	move.w	d0,-(sp)			| handle
	move.w	#0x3F,-(sp)
	trap	#1
	add.l	#12,sp
	
	| fclose
	move.l	#handle, a0	
	move.l	(a0), d0
	
	move.w	d0,-(sp)			| handle
	move.w	#0x3E,-(sp)
	trap	#1
	addq.l	#4,sp
	
.endif
	
| at this point the driver should be loaded at the location pointed by pDriver

|--------------------------------
| first fill the correct into in the Base Page
	move.l	a4, a0				| read the value stored at pDriver
	add.l	#2, a0				| a0 now points to prgHead->tsize
	
	move.l	a3, a1				| get pBasePage to A1, a1 now points to pBasePage->lowtpa
	
	move.l	(a1),d0				| d0 = pBasePage->lowtpa
	add.l	#256,d0				| d0 += 256
	add.l	#8, a1				| a1 now points to pBasePage->tbase
	move.l	d0, (a1)+			| basePage->tbase = basePage->lowtpa + 256;  and a1 points now to pBasePage->tlen
	move.l	d0, d5				| d5 = basePage->tbase, save it for the fixup loop usage
	
	move.l	(a0), d1			| d1 = prgHead->tsize
	move.l	(a0)+, (a1)+		| pBasePage->tlen = prgHead->tsize;  a0 points to prgHead->dsize and a1 points to pBasePage->dbase
	
	add.l	d1, d0				| d0 = basePage->tbase + prgHead->tsize
	move.l	d0, (a1)+			| basePage->dbase = basePage->tbase + prgHead->tsize; a1 points to pBasePage->dlen

	move.l	(a0), d1			| d1 = prgHead->dsize
	move.l	(a0)+, (a1)+		| basePage->dlen = prgHead->dsize;  a0 points to prgHead->bsize and a1 points to pBasePage->bbase

	add.l	d1, d0				| d0 = basePage->dbase + prgHead->dsize
	move.l	d0, (a1)+			| basePage->bbase = basePage->dbase + prgHead->dsize; a1 points to pBasePage->blen
	move.l	d0, a2				| a2 = basePage->bbase, save it for clearing BSS section
	
	move.l	(a0), d1			| d1 = prgHead->bsize
	move.l	(a0)+, (a1)			| basePage->blen = prgHead->bsize;  a0 points to prgHead->ssize
	move.l	d1, d6				| d6 = bsize, save it for clearing BSS section
	
|--------------------------------
| now do the fixup for the loaded text position	
	move.l	(a0), d2			| d2 = prgHead->ssize
	add.l	d2, d0				| d0 = basePage->bbase + prgHead->ssize;  == BYTE *fixups

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
	
	
| Finally execute the code using Pexec()
	move.l	a3, d0				| d0 = pointer to base page which was created by the 1st Pexec() call

	moveq	#0, d1				| d1 = 0
	
	move.l	d1, -(sp)			| envstr  = 0
	move.l	d0, -(sp)			| cmdline = pointer to base page which should be executed
	move.l	d1, -(sp)			| fname   = 0
	move.w	#4, -(sp)			| mode: PE_GO -- just execute the code
	move.w	#0x4b, -(sp)		| Pexec()
	trap	#1
	add.l	#16, sp

	| now in this point the program should be executed, and the cpu will return here after finishing the program
	
	bra		end_good			| now finish with a good result
|--------------------------------------------------------------------------------------------------------------------	

	
| jump here to free the memory and finish on fail	
free_end_fail:
|	move.l	a3, d0			

| TODO: should we Mfree the base page allocated by Pexec(), or will that be handled by TOS?	

| jump here to finish on fail without freeing the memory
end_fail:
end_good:	

.ifdef PRG
	| the following will not be needed in the real boot sector
	| return from supervisor mode
	move.l #pStack, a0
	
	move.l (a0), -(sp)
	move.w #0x20,-(sp)
	trap #1
	addq.l #6,sp
.endif
				
	movem.l	(sp)+, d1-d7/a0-a6	| restore register content
	

.ifdef FAKEJSR
	rts
.endif

	
.ifdef PRG			| end of code when run as normal app: terminate
	clr.w	-(sp)	| terminate
	trap	#1
.else				| end of code when run from boot sector: rts
	rts				| return to calling code
.endif
	
|--------------------------------------------------------------------------------------------------------------------	
.ifndef FROMFILE

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

	move.w	d2, d0			| copy in the 0th cmd byte, which is SCSI READ SECTOR + ACSI ID
	swap	D0				| push the command and acsi id to upper word of D0
	move.w	#0x008A,D0		| PIO write 0, with NO_DMA | HDC | A0 -- A1 high again

	bsr		pioWrite		| write cmd[0] and wait for IRQ
	tst.w	d0				
	bne.s	dmr_fail		| if d0 != 0, error, exit on timeout

	| send cmd[1]
	move.l	#0x00008A,D0	| PIO write 0, with NO_DMA | HDC | A0 -- A1 high again
	bsr		pioWrite		| write cmd[1] and wait for IRQ
	tst.w	d0				
	bne.s	dmr_fail		| if d0 != 0, error, exit on timeout

	| send cmd[2]
	move.l	#0x00008A,D0	| PIO write 0, with NO_DMA | HDC | A0 -- A1 high again
	bsr		pioWrite		| write cmd[2] and wait for IRQ
	tst.w	d0				
	bne.s	dmr_fail		| if d0 != 0, error, exit on timeout

	| send cmd[3]
	move.w	d1, d0			| move current sector number to d0
	swap	d0
	move.w	#0x008A, d0		| d0 now contains curent sector number + with NO_DMA | HDC | A0 -- A1 high again

	bsr		pioWrite		| write cmd[3] and wait for IRQ
	tst.w	d0				
	bne.s	dmr_fail		| if d0 != 0, error, exit on timeout

	| send cmd[4]
	move.l	#0x01008A,D0	| PIO write 1 (sector count), with NO_DMA | HDC | A0 -- A1 high again
	bsr		pioWrite		| write cmd[4] and wait for IRQ
	tst.w	d0				
	bne.s	dmr_fail		| if d0 != 0, error, exit on timeout

	
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
	swap	d0
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

.endif
| ------------------------------------------------------
	.data
| This is the configuration which will be replaced before sending the sector to ST. 
| The format is: 'XX'  AcsiId  SectorCount 
config:		dc.l			0x58580020			

.ifdef PRG
pStack:		dc.l			0	
.endif

.ifdef FROMFILE	
fname:		.ascii			"M:\\CEDD\\CEDD.PRG"
handle:		dc.l			0
.endif
| ------------------------------------------------------	
