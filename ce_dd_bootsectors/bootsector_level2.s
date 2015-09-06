    .text

|------------------------------------------------------------------------------------    
| Bootsector - Level 2
| The driver is loaded in RAM, so it's time to do fixup, BSS cleaning and execution.
|
| Parameters:
| A2 -- pointer where the driver starts
|------------------------------------------------------------------------------------

| write out title string
    pea     str(pc)
    move.w  #0x09,-(sp)
    trap    #1
    addq.l  #6,sp

|----------------------------
| now do the fixup for the loaded text position 

    movea.l a2,a0               | 
    lea     28(a0),a1           | A1 = points to text segment (prg header of size 0x1c was skipped) 
    move.l  a1,d5               | D5 = A1 (tbase)
    adda.l  2(a0),a1            | A1 += text size
    adda.l  6(a0),a1            | A1 += data size
    adda.l  14(a0),a1           | A1 += symbol size (most probably 0) == BYTE *fixups
                                
    move.l  (a1)+, d1           | read fixupOffset, a1 now points to fixups array
    beq.s   skipFixing          | if fixupOffset is 0, then skip fixing
    
    add.l   d5, d1              | d1 = basePage->tbase + first fixup offset
    move.l  d1, a0              | a0 = fist fixup pointer
    moveq   #0, d0              | d0 = 0
    
fixupLoop:
    add.l   d5, (a0)            | a0 points to place which needs to be fixed, so add text base to that
getNextFixup:
    move.b  (a1)+, d0           | get next fixup to d0  
    beq.s   skipFixing          | if next fixup is 0, then we're finished
    
    cmp.b   #1, d0              | if the fixup is not #1, then skip to justFixNext
    bne.s   justFixupNext
    
    add.l   #0x0fe, a0          | if the fixup was #1, then we add 0x0fe to the address which needs to be fixed and see what is the next fixup
    bra.s   getNextFixup
    
justFixupNext:
    add.l   d0, a0              | new place which needs to be fixed = old place + fixup 
    bra.s   fixupLoop

| code will get here either after the fixup is done, or when the fixup was skipped  
skipFixing:
    
|--------------------------------

    | clear the BSS section of prg

    movea.l a2,a0               | A0 points to start of driver 
    lea     28(a0),a1           | A1 = points to text segment (prg header of size 0x1c was skipped) 
    adda.l  2(a0),a1            | add text size
    adda.l  6(a0),a1            | add data size = bss segment starts here
    move.l  10(a0),d0           | d0.l = size of bss segment
    beq.b   bss_done
bss_loop:
    clr.b   (a1)+               
    subq.l  #1,d0
    bne.b   bss_loop
bss_done:

    move.l  #0x12345678,-(sp)   | store magic number for the startup code of driver to know that there's no basepage (meaning no base page)

    move.l  a2, a1              | A2 and A1 now point to driver
    sub.l   #512, a1            | A1 points to start of this level 2 bootsector (512 bellow the driver) - start of allocated RAM
    move.l  (a1),-(sp)          | store pointer to allocated RAM on stack

    add.l   #8, sp

    lea     256(a0),a1          | A1 = points to text segment + some offset to the real start of the code
    jmp     (a1)                | jump to the code, but it won't return here

    |-----------------------------

    .data
    .even
str:    .ascii  "\n\rCE Level 2 bootsector\n\r\0"



    