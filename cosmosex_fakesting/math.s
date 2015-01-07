	.text
	.globl	___divsi3
___divsi3:
	movem.l	d1/d2, sp@-

	moveq	# (1), d2	/* sign of result stored in d2 (=1 or =-1) */
	movel	sp@(12+4), d1	/* d1 = divisor */
	jpl	L1
	negl	d1
#ifndef __mcoldfire__
	negb	d2		/* change sign because divisor <0  */
#else
	negl	d2		/* change sign because divisor <0  */
#endif
L1:	movel	sp@(8+4), d0	/* d0 = dividend */
	jpl	L2
	negl	d0
#ifndef __mcoldfire__
	negb	d2
#else
	negl	d2
#endif

L2:	movel	d1, sp@-
	movel	d0, sp@-
	bsr     __udivsi3	/* divide abs(dividend) by abs(divisor) */
	addql	# (8), sp

	tstb	d2
	jpl	L3
	negl	d0

L3:	movem.l	sp@+, d1/d2
	rts

/********************************************************************/

	.text
	.globl	___modsi3
___modsi3:
    move.l d1,-(sp)
	movel	sp@(8+4), d1	/* d1 = divisor */
	movel	sp@(4+4), d0	/* d0 = dividend */
	movel	d1, sp@-
	movel	d0, sp@-
	bsr ___divsi3
	addql	# (8), sp
	movel	sp@(8+4), d1	/* d1 = divisor */
|#ifndef __mcoldfire__
	movel	d1, sp@-
	movel	d0, sp@-
	bsr __mulsi3	/* d0 = (a/b)*b */
	addql	# (8), sp
|#else
|	mulsl	d1,d0
|#endif
	movel	sp@(4+4), d1	/* d1 = dividend */
	subl	d0, d1		/* d1 = a - (a/b)*b */
	movel	d1, d0
    move.l (sp)+,d1
	rts

/********************************************************************/

	.text
	.globl	__udivsi3
__udivsi3:
|#ifndef __mcoldfire__
	movem.l	d1/d2, sp@-
	movel	sp@(12+4), d1	/* d1 = divisor */
	movel	sp@(8+4), d0	/* d0 = dividend */

	cmpl	# (0x10000), d1 /* divisor >= 2 ^ 16 ?   */
	jcc	.L3		/* then try next algorithm */
	movel	d0, d2
	clrw	d2
	swap	d2
	divu	d1, d2          /* high quotient in lower word */
	movew	d2, d0		/* save high quotient */
	swap	d0
	movew	sp@(10+4), d2	/* get low dividend + high rest */
	divu	d1, d2		/* low quotient */
	movew	d2, d0
	jra	L6

.L3:	movel	d1, d2		/* use d2 as divisor backup */
L4:	lsrl	# (1), d1	/* shift divisor */
	lsrl	# (1), d0	/* shift dividend */
	cmpl	# (0x10000), d1 /* still divisor >= 2 ^ 16 ?  */
	jcc	L4
	divu	d1, d0		/* now we have 16-bit divisor */
	andl	# (0xffff), d0 /* mask out divisor, ignore remainder */

/* Multiply the 16-bit tentative quotient with the 32-bit divisor.  Because of
   the operand ranges, this might give a 33-bit product.  If this product is
   greater than the dividend, the tentative quotient was too large. */
	movel	d2, d1
	mulu	d0, d1		/* low part, 32 bits */
	swap	d2
	mulu	d0, d2		/* high part, at most 17 bits */
	swap	d2		/* align high part with low part */
	tstw	d2		/* high part 17 bits? */
	jne	L5		/* if 17 bits, quotient was too large */
	addl	d2, d1		/* add parts */
	jcs	L5		/* if sum is 33 bits, quotient was too large */
	cmpl	sp@(8+4), d1	/* compare the sum with the dividend */
	jls	L6		/* if sum > dividend, quotient was too large */
L5:	subql	# (1), d0	/* adjust quotient */

L6:	movem.l	sp@+, d1/d2
	rts

|#else /* __mcoldfire__ */
|
|/* ColdFire implementation of non-restoring division algorithm from
|   Hennessy & Patterson, Appendix A. */
|	link	a6,# (-12)
|	moveml	d2-d4,sp@
|	movel	a6@(8),d0
|	movel	a6@(12),d1
|	clrl	d2		| clear p
|	moveq	# (31),d4
|.L1:	addl	d0,d0		| shift reg pair (p,a) one bit left
|	addxl	d2,d2
|	movl	d2,d3		| subtract b from p, store in tmp.
|	subl	d1,d3
|	jcs	.L2		| if no carry,
|	bset	# (0),d0	| set the low order bit of a to 1,
|	movl	d3,d2		| and store tmp in p.
|.L2:	subql	# (1),d4
|	jcc	.L1
|	moveml	sp@,d2-d4	| restore data registers
|	unlk	a6		| and return
|	rts
|#endif /* __mcoldfire__ */

/********************************************************************/

	.text
	.globl	__mulsi3
__mulsi3:
    move.l d1,-(sp)
	movew	sp@(4+4), d0	/* x0 -> d0 */
	muluw	sp@(10+4), d0	/* x0*y1 */
	movew	sp@(6+4), d1	/* x1 -> d1 */
	muluw	sp@(8+4), d1	/* x1*y0 */
|#ifndef __mcoldfire__
	addw	d1, d0
|#else
|	addl	d1, d0
|#endif
	swap	d0
	clrw	d0
	movew	sp@(6+4), d1	/* x1 -> d1 */
	muluw	sp@(10+4), d1	/* x1*y1 */
	addl	d1, d0
    move.l (sp)+,d1

	rts

