				AREA    |.text|, CODE, READONLY


gpiob_odr		EQU     0x40010c0c
gpioa_bsrr		EQU     0x40010810
gpioa_brr		EQU     0x40010814
exti_pr			EQU		0x40010414
aDMA			EQU		0x0004
aACK			EQU		0x0800

timeoutCnt		EQU		0xFFFFF

dataReadAsm		PROC
				EXPORT  dataReadAsm
				
	; params:
	; r0 - WORD * to data
	; r1 - byte count to transfer
	
	push	{r2-r9,lr}		; store registers and return address on stack

	; setup registers before the loop
	ldr		r4, =gpiob_odr	; r4 = gpiob_odr
	ldr		r5, =gpioa_bsrr	; r5 = gpioa_bsrr
	ldr		r6, =aDMA		; r6 = aDMA
	ldr		r7, =exti_pr	; r7 = EXTI->PR
	ldr		r8, =timeoutCnt	; r8 = time out count down
	ldr		r9, =aACK		; r9 = aACK
	
	; read first (next) word, split it to BYTEs, send it to Atari
readNextWord	
	ldrh	r2,[r0]			; reads 2 BYTEs of data, e.g. 0x0102
	add		r0,r0,#2		; move to next WORD

	; write upper byte to ODR
	lsr		r3,r2,#8		; R3 = R2 >> 8; that is upper byte (e.g. 0x01)
	str		r3,[r4]			; gpiob_odr = upper byte

	; toggle aDMA (H, L)
	str		r6,[r5]			; gpioa_bsrr = aDMA  --- aDMA to HIGH
	str		r6,[r5, #4]		; gpioa_brr  = aDMA  --- aDMA to LOW
	
	; wait until ACK arrives (or timeout occures)
waitAck1
	subs	r8,r8,#1		; r8--  (timeout--)
	beq		onTimeout		; if r8 == 0, jump to timeoutFail

	ldr		r3,[r7]			; R3 = exti->pr
	tst		r3, #aACK		; Z is set if ACK bit is not set
	beq		waitAck1		; if ACK not set, wait for it

	str		r9,[r7]			; exti->pr = aACK - clear the flag

	; in case of ODD count of bytes to send, check for end
	subs	r1,r1,#1		; r1--
	beq		onDone

	;------------
	; now send the lower byte
	and		r2,r2,#0xff		; r2 = r2 & 0xff   (e.g. 0x02)
	str		r2,[r4]			; gpiob_odr = lower byte

	; toggle aDMA (H, L)
	str		r6,[r5]			; gpioa_bsrr = aDMA  --- aDMA to HIGH
	str		r6,[r5, #4]		; gpioa_brr  = aDMA  --- aDMA to LOW
		
	; wait until ACK arrives (or timeout occures)
waitAck2
	subs	r8,r8,#1		; r8--  (timeout--)
	beq		onTimeout		; if r8 == 0, jump to timeoutFail

	ldr		r3,[r7]			; R3 = exti->pr
	tst		r3, #aACK		; Z is set if ACK bit is not set
	beq		waitAck2		; if ACK not set, wait for it
		
	str		r9,[r7]			; exti->pr = aACK - clear the flag
		
	; check if we should send another
	subs	r1,r1,#1		; r1--
	bne		readNextWord	; if(r1 != 0), still something to send

	; if success, continue here
onDone			
	mov		r0, #1			; return value is TRUE
	pop		{r2-r9,pc}		; restore registers and return to the calling code

	; if timeout, continue here
onTimeout				
	mov		r0, #0			; return value is FALSE
	pop		{r2-r8,pc}		; restore registers and return to the calling code
			ENDP
				
				
			END
				