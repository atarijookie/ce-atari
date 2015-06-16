| ------------------------------------------------------	

	.globl  _clearCache030
	.text

_clearCache030:
    move    sr, -(sp)       | go to IPL 7
    ori     #0x700, sr      | no interrupts right now kudasai
    movec   CACR, d0        | d0 = (cache control register)
    ori.w	#0x808,d0       | dump both the D and I cache
    movec   d0, CACR        | update cache control register
    move	(sp)+,sr        | restore interrupt state
    rts
	
| ------------------------------------------------------

