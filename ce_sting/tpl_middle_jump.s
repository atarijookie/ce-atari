	.globl _main

    .globl _fun00
    .globl _fun01
    .globl _fun02
    .globl _fun03
    .globl _fun04
    .globl _fun05
    .globl _fun06
    .globl _fun07
    .globl _fun08
    .globl _fun09
    .globl _fun10
    .globl _fun11
    .globl _fun12
    .globl _fun13
    .globl _fun14
    .globl _fun15
    .globl _fun16
    .globl _fun17
    .globl _fun18
    .globl _fun19
    .globl _fun20
    .globl _fun21
    .globl _fun22
    .globl _fun23
    .globl _fun24
    .globl _fun25
    .globl _fun26
    .globl _fun27
    .globl _fun28
    .globl _fun29
    .globl _fun30
    .globl _fun31
    .globl _fun32
    .globl _fun33
    .globl _fun34
    .globl _fun35
    .globl _fun36
    .globl _fun37
    .globl _fun38
    .globl _fun39
    
    .globl _jumptable
    
| ------------------------------------------------------	
	.text

tableJump:
	movem.l d1/d2/a1/a2, -(sp)

    lea     20(sp), a0              | get pointer to first param
    move.l  a0, -(sp)               | store it on stack - it will be the 1st param of the next C _function

    add.w   d0,d0                   | d0 contains function number, now multiply it by 4 (to represent offset in table)
    add.w   d0,d0
    ext.l	d0						| zero d0's upper word
	add.l   #_jumptable, d0         | add address of table
    move.l  d0, a0
    move.l  (a0), a0                | read address from table
    
	jsr     (a0)                    | call the function

    add.l   #4, sp                  | restore stack addr
	movem.l (sp)+, d1/d2/a1/a2
    rts
| ------------------------------------------------------

_fun00: move.l  #0, d0
        jmp     tableJump

_fun01: move.l  #1, d0
        jmp     tableJump

_fun02: move.l  #2, d0
        jmp     tableJump

_fun03: move.l  #3, d0
        jmp     tableJump

_fun04: move.l  #4, d0
        jmp     tableJump

_fun05: move.l  #5, d0
        jmp     tableJump

_fun06: move.l  #6, d0
        jmp     tableJump

_fun07: move.l  #7, d0
        jmp     tableJump

_fun08: move.l  #8, d0
        jmp     tableJump

_fun09: move.l  #9, d0
        jmp     tableJump
        
_fun10: move.l  #10, d0
        jmp     tableJump

_fun11: move.l  #11, d0
        jmp     tableJump

_fun12: move.l  #12, d0
        jmp     tableJump

_fun13: move.l  #13, d0
        jmp     tableJump

_fun14: move.l  #14, d0
        jmp     tableJump

_fun15: move.l  #15, d0
        jmp     tableJump

_fun16: move.l  #16, d0
        jmp     tableJump

_fun17: move.l  #17, d0
        jmp     tableJump

_fun18: move.l  #18, d0
        jmp     tableJump

_fun19: move.l  #19, d0
        jmp     tableJump

_fun20: move.l  #20, d0
        jmp     tableJump

_fun21: move.l  #21, d0
        jmp     tableJump

_fun22: move.l  #22, d0
        jmp     tableJump

_fun23: move.l  #23, d0
        jmp     tableJump

_fun24: move.l  #24, d0
        jmp     tableJump

_fun25: move.l  #25, d0
        jmp     tableJump

_fun26: move.l  #26, d0
        jmp     tableJump

_fun27: move.l  #27, d0
        jmp     tableJump

_fun28: move.l  #28, d0
        jmp     tableJump

_fun29: move.l  #29, d0
        jmp     tableJump

_fun30: move.l  #30, d0
        jmp     tableJump

_fun31: move.l  #31, d0
        jmp     tableJump

_fun32: move.l  #32, d0
        jmp     tableJump

_fun33: move.l  #33, d0
        jmp     tableJump

_fun34: move.l  #34, d0
        jmp     tableJump

_fun35: move.l  #35, d0
        jmp     tableJump

_fun36: move.l  #36, d0
        jmp     tableJump

_fun37: move.l  #37, d0
        jmp     tableJump

_fun38: move.l  #38, d0
        jmp     tableJump

_fun39: move.l  #39, d0
        jmp     tableJump  
        