| ------------------------------------------------------

    .globl  _cart_dma_read
    .globl  _cart_status_byte
    .globl  _cart_success

    .text

_cart_dma_read:
    movem.l D0-D4/A0-A3,-(SP)   | save regs

    | input args:
    move.l  4(sp), a0       | A0 - buffer pointer to store data
    move.l  8(sp), d1       | D1 - byte count to transfer

    move.l  #0x04BA, a3     | pointer to 200 Hz timer
    move.l  (a3), d2        | current 200 HZ counter value
    add.l   #200, d2        | timeout counter value

    move.l  #0xFB0001, A1   | address for data
    move.l  #0xFB0000, A2   | address for CPLD status

| ------------------------------------------------------
    jmp     start_of_read_loop  | jump do the dbra, because it counts to -1 (not 0)

wait_for_read:
    move.b  (a2), d4        | read status
    btst    #1, d4          | test RPIwantsMore
    bne     do_read_byte    | if RPIwantsMore is 1, should transfer byte

    btst    #2, d4                  | test RPIisIdle
    bne     store_status_and_quit   | if RPIisIdle is 1, then RPi went idle and last read byte was status

    move.l  (a3), d3        | read current value of 200 Hz timer
    cmp.l   d2, d3          | compare if timeout time (d2) was reached by current time (d3)
    beq     on_timeout

    jmp     wait_for_read   | if came here, we need to wait some more

do_read_byte:
    move.b  (a1), d0        | read data from cart
    move.b  d0,(a0)+        | store data to buffer

start_of_read_loop:
    dbra    d1, wait_for_read  | check if we have something more to read

    | if we got here, we transfered whole sector, so now we need to read the status byte

wait_for_status:
    move.b  (a2), d4        | read status
    btst    #1, d4          | test RPIwantsMore
    bne     read_status     | if RPIwantsMore is 1, we can now read the status

    btst    #2, d4                  | test RPIisIdle
    bne     store_status_and_quit   | if RPIisIdle is 1, then RPi went idle and last read byte was status

    move.l  (a3), d3        | read current value of 200 Hz timer
    cmp.l   d2, d3          | compare if timeout time (d2) was reached by current time (d3)
    beq     on_timeout

    jmp     wait_for_status

read_status:
    move.b  (a1), d0        | do a PIO read (status byte)

store_status_and_quit:
    | success -- 0: false; 1 -- true

    move.b  d0, _cart_status_byte   | store status byte
    move.b  #1, _cart_success       | this was a success

    movem.l (SP)+, D0-D4/A0-A3      | restore regs
    rts

on_timeout:
    move.b  #0, _cart_success       | this was a fail
    movem.l (SP)+, D0-D4/A0-A3      | restore regs
    rts

| ------------------------------------------------------
_cart_dma_write:
    movem.l D0-D4/A0-A3,-(SP)   | save regs

    | input args:
    move.l  4(sp), a0       | A0 - buffer pointer to store data
    move.l  8(sp), d1       | D1 - byte count to transfer

    move.l  #0x04BA, a3     | pointer to 200 Hz timer
    move.l  (a3), d2        | current 200 HZ counter value
    add.l   #200, d2        | timeout counter value

    move.l  #0xFB0001, A1   | address for data
    move.l  #0xFB0000, A2   | address for CPLD status

| ------------------------------------------------------
    jmp     start_of_write_loop     | jump do the dbra, because it counts to -1 (not 0)

wait_for_write:
    move.b  (a2), d4        | read status
    btst    #1, d4          | test RPIwantsMore
    bne     do_write_byte   | if RPIwantsMore is 1, should transfer byte

    btst    #2, d4                  | test RPIisIdle
    bne     store_status_and_quit   | if RPIisIdle is 1, then RPi went idle and last read byte was status

    move.l  (a3), d3        | read current value of 200 Hz timer
    cmp.l   d2, d3          | compare if timeout time (d2) was reached by current time (d3)
    beq     on_timeout

    jmp     wait_for_write  | if came here, we need to wait some more

do_write_byte:
    clr.l   d0              | make sure that whole register is clear
    move.b  (a0)+, d0       | get data from buffer - in the lowest byte of the register
    move.b  (a1, d0.l), d0  | do a data write by reading from DMA write address + offset, d0 holds the last read byte

start_of_write_loop:
    dbra    d1, wait_for_write  | check if we have something more to write

    | if we got here, we transfered whole sector, so now we need to read the status byte
    jmp     wait_for_status | continue with the same end as with card_dma_read

| ------------------------------------------------------

    .bss
_cart_status_byte:  .ds.b   1
_cart_success:      .ds.b   1
