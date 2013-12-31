
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
|.equ PRG,1

.ifdef PRG
	| the following will not be needed in the real boot sector
	| mshrink()
	move.l	4(sp),a5				| address to basepage
	move.l	#10000, -(sp)			| memory size to keep 
	move.l	a5,-(sp)				|
	clr.w	-(sp)					|
	move.w	#0x4a,-(sp)				|
	trap	#1						|	
	lea.l	12(sp),sp				|
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

	move.l	#0xC800,-(sp)		| malloc 50 kB of RAM (should be enough for the driver)
	move.w	#0x48,-(sp)
	trap	#1
	addq.l	#6,sp
	
	tst.l	D0					| if malloc failed, jump to end
	beq		end_fail

	move.l	d0, pMemory			| store the original memory pointer
	add.l	#2, d0				| d0 = d0 + 2
	and.l	#0xfffffffe, d0		| remove lowest bit == make EVEN address
	move.l	d0, a1				| A1 will hold the current DMA transfer address, and it will move further by sector size
	move.l	d0, pDriver			| this is the EVEN address where the driver will be loaded
	
	move.l	#config,a2			| load the config address to A2
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

	
| at this point the driver should be loaded at the location pointed by pDriver, we should execute it	
	move.l	#pDriver, a0		| retrieve the pointer to where the driver was loaded
	move.l	(a0), a0			| read the value stored at pDriver
	
	
	
	
| TODO: execute the code at A0. Probably Pexec() ?	
	
	
	
	
| jump here to free the memory and finish on fail	
free_end_fail:
	move.l	#pMemory, a0		| retrieve original memory pointer returned by malloc
	move.l	(a0), d0			
	
	move.l	d0, -(sp)			| mfree the pMemory, we've failed
	move.w	#0x49,-(sp)
	trap	#1
	addq	#6,sp

| jump here to finish on fail without freeing the memory
end_fail:	

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
	
.ifdef PRG			| end of code when run as normal app: terminate
	clr.w	-(sp)	| terminate
	trap	#1
.else				| end of code when run from boot sector: rts
	rts				| return to calling code
.endif
	
|--------------------------
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
	bne		dmr_fail		| if d0 != 0, error, exit on timeout

	| send cmd[1]
	move.l	#0x00008A,D0	| PIO write 0, with NO_DMA | HDC | A0 -- A1 high again
	bsr		pioWrite		| write cmd[1] and wait for IRQ
	tst.w	d0				
	bne		dmr_fail		| if d0 != 0, error, exit on timeout

	| send cmd[2]
	move.l	#0x00008A,D0	| PIO write 0, with NO_DMA | HDC | A0 -- A1 high again
	bsr		pioWrite		| write cmd[2] and wait for IRQ
	tst.w	d0				
	bne		dmr_fail		| if d0 != 0, error, exit on timeout

	| send cmd[3]
	move.w	d1, d0			| move current sector number to d0
	swap	d0
	move.w	#0x008A, d0		| d0 now contains curent sector number + with NO_DMA | HDC | A0 -- A1 high again

	bsr		pioWrite		| write cmd[3] and wait for IRQ
	tst.w	d0				
	bne		dmr_fail		| if d0 != 0, error, exit on timeout

	| send cmd[4]
	move.l	#0x01008A,D0	| PIO write 1 (sector count), with NO_DMA | HDC | A0 -- A1 high again
	bsr		pioWrite		| write cmd[4] and wait for IRQ
	tst.w	d0				
	bne		dmr_fail		| if d0 != 0, error, exit on timeout

	
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
	bne		dmr_fail		| if d0 != 0, error, exit on timeout
	
	move.w	#0x08A,(A6)		| select status register
	move.w	(A5), D0		| get DMA return code
	and.w	#0x00FF, D0		| mask for error code only
	beq		dmr_success		| if D0 == 0, success!

dmr_fail:
	move.l	#0xffffffff,D0	| set error return (-1)

dmr_success:		
	move.w	#0x080,(A6)		| reset DMA chip for driver
	tst.b	D0				| test for error return
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
	beq		gotINT			| OK return
	cmp.l	Hz_200, d0		| timeout yet?
	bne		waitMore		| no? try again
	
	move.l	#0xffffffff, d0	| d0 = fail!
	rts
	
gotINT:
	move.l	#0, d0			| d0 = success!
	rts


| ------------------------------------------------------
	.data
| This is the configuration which will be replaced before sending the sector to ST. 
| The format is: 'XX'  AcsiId  SectorCount 
config:		dc.l			0x58580020			
pMemory:	dc.l			0
pDriver:	dc.l			0	
	
pStack:		dc.l			0	
| ------------------------------------------------------	
