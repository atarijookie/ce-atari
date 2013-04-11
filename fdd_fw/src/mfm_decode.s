	.section .data.ramfunc,"ax",%progbits

 	.syntax unified
 	.cpu cortex-m3
 	.thumb
 	.align	4
 	.global	mfm_append_change
 	.thumb_func

mfm_append_change:
	push	{r1, r2, r3, lr}

	// appending of flux changes
	ldr		r1, =changes		// R1 = pointer to 'changes'
	ldrb	r2, [r1]			// R2 = value   of 'changes'
	lsl		r2, r2, #1			// R2 = R2 << 1
	and		r2, r2, #3			// R2 = R2 & 3 (leave lowest 2 bits)
	orr		r2, r2, r0			// R2 = R2 | r0 (r0 is input argument)
	strb	r2, [r1]			// update 'changes' in memory

	ldr		r1, =chCount		// R1 = pointer to 'chCount'
	ldrb	r3, [r1]			// R1 = value   of 'chCount'
	add		r3, r3, #1			// R3++
	and		r3, r3, #1			// R3 = R3 & 1  -- this way it contains only 0 or 1
	strb	r3, [r1]			// update 'chCount' in memory

	cmp		r3, #0				// if chCount == 0, we got 2 changes
	bne		mac_end				// we don't have 2 changes, finish

	//--------------
	// appending of real data bits
	ldr		r1, =newByte		// R1 = pointer to 'newByte'
	ldrb	r3, [r1]			// R3 = value   of 'newByte'
	lsl		r3, r3, #1			// R3 = R3 << 1

	cmp		r2, #1				// check for NR changes in flux
	bne		mac_zero			// if changes are not NR, then it's not 1 (NN or RN means 0)

	orr		r3, r3, #1			// R3 = R3 | 1		-- append 1 to the newByte
mac_zero:
	strb	r3, [r1]			// update 'newByte' in memory

	// update and check for bits count we got
	ldr		r1, =newBits		// R1 = pointer to 'newBits'
	ldrb	r2, [r1]			// R2 = value   of 'newBits'
	add		r2, r2, #1			// R2++
	and		r2, r2, #7			// R2 = R2 & 7  -- this way it contains only 3 lowest bits
	strb	r2, [r1]			// update 'newBits' in memory

	cmp		r2, #0				// did we get 8 bits appended already?
	bne		mac_end				// no, not enough bits yet

	//------
	// storing of data in array
	ldr		r1, =cnt			// R1 = pointer to 'cnt'
	ldr.w	r2, [r1]			// R2 = value   of 'cnt'

	ldr		r1, =data			// R1 = pointer to 'data'
	add		r1, r1, r2			// R1 = R1 + offset by 'cnt'
	strb	r3, [r1]			// store newByte to data[cnt]

mac_end:
 	pop		{r1, r2, r3, lr}
 	bx		lr
//---------------------------------------
