| ------------------------------------------------------

    .globl  _cart_dma_read
    .globl  _cart_status_byte
    .globl  _cart_success

    .text

_cart_dma_read:
    movem.l D0-D3/A0-A3,-(SP)   | save regs

    | input args:
    move.l  4(sp), a0       | A0 - buffer pointer to store data
    move.l  8(sp), d1       | D1 - byte count to transfer

    move.l  #0x04BA, a3     | pointer to 200 Hz timer
    move.l  (a3), d2        | current 200 HZ counter value
    add.l   #200, d2        | timeout counter value

    move.l  #0xfbc200, A1   | address for DMA read
    move.l  #0xfbd200, A2   | address for reading CPLD status

read_another:
    move.l  (a3), d3        | read current value of 200 Hz timer
    cmp.l   d2, d3          | compare if timeout time (d2) was reached by current time (d3)
    beq     on_timeout

    move.w  (a2), d0        | read status
    btst    #1, d0          | test DRQ
    bne     read_another    | if DRQ is 1, wait some more

    move.w  (a1), d0        | read data from cart

    btst    #17, d0         | if STATUS2_DataIsPIOread is set, this wasn't data but the status byte
    bne     store_status_and_quit

    move.b  d0,(a0)+        | store data to buffer

    sub.l   #1,D1           | byteCount--
    tst.l   D1              | check if we have something more to read
    bne     read_another    | still something do to, go again

    | if we got here, we transfered whole sector, so now we need to read the status byte

get_status_byte:
    move.l  #0xfbc600, a1   | address for PIO read

wait_for_status:
    move.l  (a3), d3        | read current value of 200 Hz timer
    cmp.l   d2, d3          | compare if timeout time (d2) was reached by current time (d3)
    beq     on_timeout

    move.w  (a2), d0        | read status
    btst    #0, d0          | test INT
    bne     wait_for_status | if INT is 1, wait some more

    move.w  (a1), d0        | do a PIO read (status byte)

store_status_and_quit:
    | success -- 0: false; 1 -- true

    move.b  d0, _cart_status_byte   | store status byte
    move.b  #1, _cart_success       | this was a success

    movem.l (SP)+, D0-D3/A0-A3      | restore regs
    rts

on_timeout:
    move.b  #0, _cart_success       | this was a fail
    movem.l (SP)+, D0-D3/A0-A3      | restore regs
    rts

| ------------------------------------------------------
    .bss
_cart_status_byte:  .ds.b   1
_cart_success:      .ds.b   1
