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
    lea     dma_pointer(pc),a0
    move.l  d0,(a0)

    | crate 0th cmd byte from ACSI ID (config + 2)
    lea     config(pc),a2       | load the config address to A2
    move.b  2(a2), d2           | d2 holds ACSI ID
    lsl.b   #5, d2              | d2 = d2 << 5
    ori.b   #0x08, d2           | d2 now contains (ACSI ID << 5) | 0x08, which is READ SECTOR from ACSI ID device

    | get the sector count from (config + 3)
    clr.l   d3
    move.b  3(a2), d3           | d3 holds sector count we should transfer
    subq.l  #1, d3              | d3--, because the dbra loop will be executed (d3 + 1) times

    moveq   #1, d1              | d1 holds the current sector number. Bootsector is 0, so starting from 1 to (sector count + 1)

    jsr     dma_read            | trasnfer all the sectors from SCSI to RAM

|--------------------------------

    jmp     (a1)                | jump to the code which will do fixup and clearing BSS

    
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
| The format is : 'XX'  AcsiId  SectorCount driverRamBytes
| default values: 'XX'       0   32 (0x20)  70 kB (0x11800 bytes)

config:     .dc.l   0x58580020
driverRam:  .dc.l   0x00011800
acsiCmd:    .dc.b   0x08,0x00,0x00,0x00,0x01

str:    .ascii  "\n\rCE TT bs"
        .dc.b   13,10,0
error:  .ascii  "fail"
        .dc.b   13,10,0

    .text
    .even

| TT BYTE regs for reading
REG_DB  = 0xFFFF8781
REG_ICR = 0xFFFF8783
REG_MR  = 0xFFFF8785
REG_TCR = 0xFFFF8787
REG_CR  = 0xFFFF8789
REG_DSR = 0xFFFF878b
REG_IDR = 0xFFFF878d
REG_REI = 0xFFFF878f

| TT BYTE regs for writing
REG_ODR = 0xFFFF8781
REG_ISR = 0xFFFF8789
REG_DS  = 0xFFFF878b
REG_DTR = 0xFFFF878d
REG_DIR = 0xFFFF878f
    
flock   = 0x43E         | .W DMA chiplock variable
dskbuf  = 0x4C6         | .L IK disk buffer
Hz_200  = 0x4BA         | .L 200 Hz counter
bootmg  = 0x1234        | .W boot checksum

TCR_PHASE_DATA_OUT  = 0
TCR_PHASE_DATA_IN   = 1
TCR_PHASE_CMD       = 2
TCR_PHASE_STATUS    = 3

||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
| subroutine used to sectors to memory from SCSI device
dma_read:
    st  flock               | lock the DMA chip

|----------------------    
| SCSI SELECTION: START  

    move.l  #0xFFFF8783, a6                 | REG_ICR
  
    move.b  #TCR_PHASE_DATA_OUT, REG_TCR       | REG_TCR = data out phase
    move.b  #0, REG_ISR                     | REG_ISR = no interrupt from selection
    move.b  #0x0c,REG_ICR                      | REG_ICR = assert BSY and SEL 
    
    lea     config(pc),a2       | load the config address to A2
    move.b  2(a2), d2           | d2 holds SCSI ID
    move.b  #1, d0
    lsl.b   d2, d0              | d0 = d0 << d2
    move.b  d2, REG_DB         | set dest SCSI IDs
    
    move.b  #0x0d, REG_ICR      | assert BUSY, SEL and data bus
    
    move.b  REG_MR, d0
    andi.b  #0xfe, d0           | scsi_clrBit_TT(REG_MR, MR_ARBIT); 
    move.b  d0, REG_MR
        
    move.b  REG_ICR, d0
    andi.b  #0xf7, d0           | scsi_clrBit_TT(REG_ICR, (1 << 3))
    move.b  d0, REG_ICR

waitForSelEnd:    
    move.b  REG_CR, d0
    andi.b  #0x40, d0           | get only ICR_BUSY
    tst.b   d0
    beq     waitForSelEnd       | wait until ICR_BUSY is set (loop if zero)

    move.b  #0, REG_ICR         | clear SEL and data bus assertion
    
| SCSI SELECTION: END
|----------------------    
| SCSI COMMAND: START    
    move.b  #TCR_PHASE_CMD, REG_TCR | set COMMAND PHASE (assert C/D)
    move.b  #1, REG_ICR             | assert data bus
    
    move.b  #0x08, d0
    jsr     pioWrite        | 0x08

    jsr     pioWrite        | 0x00
    jsr     pioWrite        | 0x00
    jsr     pioWrite        | 0x00

    lea     config(pc),a2   | load the config address to A2
    clr.l   d3
    move.b  3(a2), d3       | d3 holds sector count we should transfer
    move.b  d3, d0
    jsr     pioWrite        | sector count

    jsr     pioWrite        | 0x00
    
| SCSI COMMAND: END
|----------------------
| SCSI DATA IN: START    
    lsl.l   #8, d3              
    lsl.l   #1, d3              | d3 = d3 * 512 (d3 = d3 << 9) -- convert sector count to byte count
    
    subq.l  #1, d3              | d3--, because the dbra loop will be executed (d3 + 1) times
    
    move.b  #0, REG_ICR                     | deassert the data bus
    move.b  #TCR_PHASE_DATA_IN, REG_TCR     | read - set DATA IN  phase
    move.b  REG_REI, d0                     | clear potential interrupt
    
dataInLoop:    
    jsr     pioRead                         | get data by PIO reads into D1
    move.b  d1, (a1)+
    dbra    d3, dataInLoop
    
| SCSI DATA IN: END
|------------------------    
| SCSI STATUS: START

    move.b  #TCR_PHASE_STATUS, REG_TCR         | set STATUS phase
    move.b  REG_REI, d0                     | clear potential interrupt

    jsr     pioRead
| SCSI STATUS: END
|------------------------    
    sf      flock                           | unlock DMA chip
    rts
||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

|------------------------    
| d1 = pioRead - read byte from SCSI bus, doing PIO REQ ACK handshake
pioRead:
    jsr     wait4req1                       | wait for REQ to appear
    move.b  REG_DB, d1                      | read byte into D1

    jsr     doAck                           | set ACK, wait for REQ to disappear
    rts
|------------------------    
| pioWrite(d0) - write byte to SCSI bus, doing PIO REQ ACK handshake
pioWrite:
    jsr     wait4req1       | wait until REQ bit appears
    move.b  d0, REG_DB      | write byte out to data bus
    
    jsr     doAck
    move.b  #0, d0          | clear this byte
    rts
    
|------------------------    
| doAck -- set ACK, wait until REQ disappear, then remove ACK and return
doAck:                          
    move.b  REG_ICR, d0
    ori.b   #0x11, d0
    move.b  d0, REG_ICR     | assert ACK (and data bus)

    jsr     wait4req0       | wait until REQ bit disppears
    
    move.b  REG_ICR, d0
    andi.b   #0xef, d0
    move.b  d0, REG_ICR     | clear ACK 
    
    rts
|------------------------    
| wait until REQ bit appears
wait4req1:
    move.b  REG_CR, d1      | read register
    andi.b  #0x20, d1       | get only REQ bit
    tst.b   d1
    beq     wait4req1       | if REQ is not there, wait
    rts
|------------------------    
| wait until REQ bit disappears
wait4req0:
    move.b  REG_CR, d1      | read register
    andi.b  #0x20, d1       | get only REQ bit
    tst.b   d1
    bne     wait4req0       | while REQ is still there, wait
    rts

|----------------------------    

    