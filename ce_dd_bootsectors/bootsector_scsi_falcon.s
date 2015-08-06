    .text
|----------------------------------------------------------------
| CosmosEx bootsector Level 1 for TT through its SCSI interface
|----------------------------------------------------------------

| write out title string
    pea     str(pc)
    move.w  #0x09,-(sp)
    trap    #1
    addq.l  #6,sp

    lea     config(pc),a6       | load the config address to A6, won't load this anymore
    
| malloc RAM, by default 70 kB, but is configurable
    move.l  (a6)+, -(sp)        | read how much RAM we should allocate
    move.w  #0x48,-(sp)         | Malloc()
    trap    #1                  |
    addq.l  #6,sp               |
    tst.l   d0
    beq     fail

| store memory pointer to A1 and dma_pointer
    movea.l d0, a1                  | A1 is the DMA address
    movea.l d0, a0                  | A0 is also the pointer to DMA address
    
|------------
| fill A4 and A5 with pointers to getReg() and setReg() functions for the correct machine 
    lea     getReg_TT(pc), a4       | pointer to getReg for TT
    lea     setReg_TT(pc), a5       | pointer to setReg for TT

    move.l  0x04f2.w, a3            | get pointer to TOS
    move.w  2(a3), d0               | TOS +2: TOS version

    lsr.w   #8, d0                  | TOS major version to lowest byte
    cmp.b   #4, d0                  | TOS major 4 means Falcon TOS
    bne     notFalcon               | if it's not TOS version 4, it's not Falcon

itsAFalcon:
    lea     getReg_Falcon(pc), a4   | pointer to getReg for Falcon
    lea     setReg_Falcon(pc), a5   | pointer to setReg for Falcon
notFalcon:
|------------
    
    movea.w #0x8780, a3             | pointer to base of TT SCSI register
    
    clr.l   d4
    move.b  1(a6), d4               | d4 holds sector count we should transfer (e.g. 0x20)
    subq.l  #1, d4                  | dbra branches +1 more than specified

    move.b  #1, 1(a6)               | 1(a6) now holds starting sector (and later the current sector)
    
readNextSector:                     | transfer all the sectors from SCSI to RAM - address A1, status to D5
    bsr.b   dma_read                
    dbra    d4, readNextSector
|--------------------------------
| driver is loaded in RAM, now to do the rest

    move.l  a0,   a2            | A2 points to Level 2 bootsector
    add.l   #512, a2            | A2 now points to driver (512 bytes after L2 bootsector)
    
    jmp     (a0)                | jump to Level 2 bootsector, which will do fixup and clearing BSS
    
    |-----------------------------
    | if failed....
    
fail1:  
    move.l  a0, -(sp)           | store allocated ram pointer on stack
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

| This is the configuration which will be replaced before sending the sector to ST.
| The format is : 'XX'  driverRamBytes        ScsiIdBits  SectorCount 
| default values: 'XX'  70 kB (0x11800 bytes)      1        32 (0x20)  

configTag:  .dc.w   0x5858
config:     .dc.l   0x00011800
            .dc.b   0x01, 0x20

str:    .ascii  "\n\rCE SCSI\0"
error:  .ascii  ":(\n\r\0"

    .text
    .even
    
flock   = 0x43E         | .W DMA chiplock variable
dskbuf  = 0x4C6         | .L IK disk buffer
Hz_200  = 0x4BA         | .L 200 Hz counter
bootmg  = 0x1234        | .W boot checksum

||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
| subroutine used to sectors to memory from SCSI device
dma_read:
    st      flock               | lock the DMA chip

|----------------------    
| SCSI SELECTION: START  

    moveq   #0x07, d0           | REG_TCR: data out phase
    jsr     (a5)                | jsr setReg
    
    moveq   #0x09, d0           | REG_ISR = no interrupt from selection
    jsr     (a5)                | jsr setReg
    
    move.w  #0x0c03, d0         | REG_ICR = assert BSY and SEL 
    jsr     (a5)                | jsr setReg
    
    move.b  (a6), d0            | D0 = 0x00ID
    lsl.w   #8, d0              | d0 = d0 << 8      -- 0xID00
    ori.b   #1, d0              | d0 = d0 | 0x01    -- 0xID01
    jsr     (a5)                | jsr setReg -- set dest SCSI IDs

    move.w  #0x0d03, d0         | assert BUSY, SEL and data bus
    jsr     (a5)                | jsr setReg

    move.w  #0xfe05, d7         | REG_MR: clear MR_ARBIT
    bsr.w   clrBit

    move.w  #0xf703, d7         | REG_ICR: clear ICR_BSY
    bsr.w   clrBit

waitForSelEnd:    
    moveq   #0x09, d0           | get REG_CR
    jsr     (a4)                | jsr getReg
    andi.b  #0x40, d0           | get only ICR_BUSY
    beq     waitForSelEnd       | wait until ICR_BUSY is set (loop if zero)

    moveq   #0x03, d0           | REG_ICR: clear SEL and data bus assertion
    jsr     (a5)                | jsr setReg
    
| SCSI SELECTION: END
|----------------------    
| SCSI COMMAND: START
    move.w  #0x0207, d0         | REG_TCR: set COMMAND PHASE (assert C/D)
    jsr     (a5)                | jsr setReg

    move.w  #0x0103, d0         | REG_ICR: assert data bus
    jsr     (a5)                | jsr setReg
    
    moveq   #8, d6
    bsr.b   pioWrite            | cmd[0]: 0x08 -- SCSI CMD: READ(8)

    bsr.b   pioWrite            | cmd[1]: 0x00
    bsr.b   pioWrite            | cmd[2]: 0x00
    
    move.b  1(a6),d6            | starting sector to d6
    addq    #1, 1(a6)           | starting_sector++
    bsr.b   pioWrite            | cmd[3]: start reading from sector D6

    moveq   #1, d6
    bsr.b   pioWrite            | cmd[4]: sector count
    
    bsr.b   pioWrite            | cmd[5]: 0x00
    
| SCSI COMMAND: END
|----------------------
| SCSI DATA IN: START    
    moveq   #2, d3  
    lsl.l   #8, d3              | d3 = 0x200 - count of bytes we want to transfer
    subq.l  #1, d3              | d3--, because the dbra loop will be executed (d3 + 1) times
    
    moveq   #0x03, d0           | REG_ICR: deassert the data bus
    jsr     (a5)                | jsr setReg

    move.w  #0x0107, d0         | REG_TCR: set DATA IN  phase
    jsr     (a5)                | jsr setReg

    moveq   #0x0f, d0           | REG_REI: clear potential interrupt
    jsr     (a4)                | jsr getReg
    
dataInLoop:    
    bsr.b   pioRead             | get data by PIO reads into D6
    move.b  d6, (a1)+
    dbra    d3, dataInLoop
    
| SCSI DATA IN: END
|------------------------    
| SCSI STATUS: START

    move.w  #0x0307, d0         | REG_TCR: set STATUS phase
    jsr     (a5)                | jsr setReg
    
    moveq   #0x0f, d0           | REG_REI: clear potential interrupt
    jsr     (a4)                | jsr getReg

    bsr.b   pioRead             | read status into D6
    move.b  d6, d5              | copy status to D5, because D6 will be destroyed when reading MSG IN
    
    move.w  #0x0707, d0         | REG_TCR: set MESSAGE IN phase
    jsr     (a5)                | jsr setReg

    bsr.b   pioRead             | read message into D1
    
| SCSI STATUS: END
|------------------------    
    sf      flock               | unlock DMA chip
    rts
||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

|------------------------    
| d6 = pioRead - read byte from SCSI bus, doing PIO REQ ACK handshake
pioRead:
    bsr.b   wait4req1           | wait for REQ to appear
    
    moveq   #0x01, d0           | REG_DB: read byte into D0
    jsr     (a4)                | jsr getReg
    move.b  d0,d6               | copy D0 into D6, because D0 will be destroyed in bsr doAck

    bsr.b   doAck               | set ACK, wait for REQ to disappear
    rts
|------------------------    
| pioWrite(d6) - write byte to SCSI bus, doing PIO REQ ACK handshake
pioWrite:
    bsr.b   wait4req1           | wait until REQ bit appears
    
    move.b  d6, d0              | copy the input data to D0
    lsl.w   #8, d0              | d0 = d0 << 8
    ori.w   #0x01, d0           | d0 = d0 | 0x01    -- D0 is  DATA 01
    jsr     (a5)                | jsr setReg -- REG_DB: write byte out to data bus
    
    bsr.b   doAck
    move.b  #0, d6              | clear this byte
    rts
    
|------------------------    
| doAck -- set ACK, wait until REQ disappear, then remove ACK and return
doAck:
    move.w  #0x1103, d7     | REG_ICR: assert ACK (and data bus)
    bsr.b   setBit
    
    bsr.b   wait4req0       | wait until REQ bit disppears
    
    move.w  #0xef03, d7     | REG_ICR: clear ACK
    bsr.b   clrBit
    
    rts
|------------------------    
| wait until REQ bit appears
wait4req1:
    moveq   #0x09, d0       | read REG_CR register
    jsr     (a4)            | jsr getReg
    
    andi.b  #0x20, d0       | get only REQ bit
    beq     wait4req1       | if REQ is not there, wait
    rts
|------------------------    
| wait until REQ bit disappears
wait4req0:
    moveq   #0x09, d0       | read REG_CR register
    jsr     (a4)            | jsr getReg

    andi.b  #0x20, d0       | get only REQ bit
    bne     wait4req0       | while REQ is still there, wait
    rts

|----------------------------
| to set (OR) new bits to register, fill D7 with: hi - OR_BITS, low: register_offset
setBit: 
    move.b  d7, d0          | get register offset into D0 (D7 is: OR_BITS REG_OFFSET)
    jsr     (a4)            | jsr getReg
    lsl.w   #8, d0          | d0 = d0 << 8    -- original register value to upper nibble (D0 is: ORIGINAL_VALUE 00)
    or.b    d7, d0          | d0 = d7 | d0    -- D0 is NEW_VALUE REG_OFFSET
    jsr     (a5)            | jsr setReg -- write new value to register
    rts

|----------------------------
| to clear (AND) bits in register, fill D7 with: hi - AND_BITS, low: register_offset
clrBit: 
    move.b  d7, d0          | get register offset into D0 (D7 is: OR_BITS REG_OFFSET)
    jsr     (a4)            | jsr getReg
    lsl.w   #8, d0          | d0 = d0 << 8    -- original register value to upper nibble (D0 is: ORIGINAL_VALUE 00)
    and.w   d7, d0          | d0 = d0 & d7    -- do 'AND' on original value
    move.b  d7, d0          | store the REG_OFFSET to lower nibble of D0
    jsr     (a5)            | jsr setReg -- write new value to register
    rts
    
||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

setReg_Falcon:
    bsr.b   selectNcrReg    | select NCR register by writing to WDC register

    lsr     #8, d0          | D0 - lowest byte contains value which should go to register
    move.w  d0, 0x8604.w    | WDC = value which should go to NCR register
    rts
    
|----------------------------        
getReg_Falcon:
    bsr.b   selectNcrReg    | select NCR register by writing to WDC register

    move.w  0x8604.w, d0    | read from WDC = get value from NCR register
    rts

|----------------------------        
| convert the register offset to value which should go to WDC register to select the right NCR register    
selectNcrReg:
    clr.l   d1
    move.b  d0, d1          | D1 - lowest byte contains register offset
    lsr.b   #1, d1          | D1 = D1 / 2, because Falcon offset is half of TT offset
    move.b  #0x88, d2       | D2 = 0x88 = base of register switch value
    add.b   d2, d1          | D1 = 0x88 + (register_offset / 2)
    move.w  d1, 0x8606.w    | WDL = select which NCR register to access
    rts

||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
    
setReg_TT:
   clr.l   d1
   move.b  d0, d1          | D1 - lowest byte contains register offset
   lsr     #8, d0          | D0 - lowest byte contains value which should go to register
   move.b  d0, (d1, a3)    | store value to register
   rts

|----------------------------    

getReg_TT:
   clr.l   d1
   move.b  d0, d1           | D1 - lowest byte contains register offset
   move.b  (d1, a3), d0     | get value from register
   rts

||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

