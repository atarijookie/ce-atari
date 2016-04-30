|this is derived from the original TOS approach
|but we don't use the IKBD interrupt 

				.globl	_ikbdwc
				.globl	_ikbdget
				.globl	_ikbdtxready
				.globl  _ikbdtxdata
				.globl  _ikbdrxdata
				.globl 	_ikbdtimeoutflag
| ------------------------------------------------------
				.text
| ------------------------------------------------------ 
_ikbdwc:
				movem.l D0-D3/A0/A1,-(SP)
				move.b	_ikbdtxdata,D1

				lea		0x04BA.w,A0		|TOS 200Hz counter
				lea		0xfffffc02.w,A1	|point to ikbd data register 
				sf 		_ikbdtimeoutflag

				bsr 	iktxready

				|check 200Hz
				move.l	(A0),D3
				add.l	#192,D3		|wait for 192 200Hz interrupts
ikput2:			cmp.l 	(A0),D3
				bgt.s	ikput2		|no - loop

				move.b	D1,(A1)	| now put character
				movem.l (SP)+,D0-D3/A0/A1
				rts						|done for now
_ikbdget:
				movem.l D0-D3/A0/A1,-(SP)

				lea		0x04BA.w,A0		|TOS 200Hz counter
				lea		0xfffffc02.w,A1	|point to ikbd data register 
				sf 		_ikbdtimeoutflag

				move.l	(A0),D3
				add.l	#400,D3		| timeout after 2 seconds
ikget1:
				move.b	-2(A1),D2		|grab keyboard status
|				cmp.l 	(A0),D3
|				blt.s 	ikbdtimeout
				btst	#0,D2
				beq.b	ikget1		| not ready

				move.b 	(A1),_ikbdrxdata
						
				movem.l (SP)+,D0-D3/A0/A1
				rts

_ikbdtxready:
				movem.l D0-D3/A0/A1,-(SP)

				lea		0x04BA.w,A0		|TOS 200Hz counter
				lea		0xfffffc02.w,A1	|point to ikbd data register 
				sf 		_ikbdtimeoutflag

				bsr 	iktxready

				movem.l (SP)+,D0-D3/A0/A1
				rts

iktxready:
				move.l	(A0),D3
				add.l	#4000,D3		|timeout after 2 seconds
ikput1:
				move.b	-2(A1),D2		|grab keyboard status
				cmp.l 	(A0),D3
				blt.s 	ikbdtxtimeout
				btst	#1,D2
				beq.b	ikput1		|not ready
				rts
ikbdtxtimeout:
				st 		_ikbdtimeoutflag
				rts
ikbdtimeout:
				st 		_ikbdtimeoutflag
				movem.l (SP)+,D0-D3/A0/A1
				rts


	.bss 
_ikbdtxdata: 		DS.B 	1
_ikbdrxdata:		DS.B 	1
_ikbdtimeoutflag: 	DS.B 	1
	.even
