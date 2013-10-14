//---------------------------------------
#include <defBF531.h>
#include "pin_defines.h"

.extern _brStat;
.extern _InitStorage;
.extern _ListStorage;

// defines for offset addressing of FIO_FLAG registers
#define		DD_ofs			(FIO_FLAG_D - FIO_FLAG_D)
#define		DC_ofs			(FIO_FLAG_C - FIO_FLAG_D)
#define		DS_ofs			(FIO_FLAG_S - FIO_FLAG_D)
#define		DT_ofs			(FIO_FLAG_T - FIO_FLAG_D)

#define		DIR_INEN_ofs	(FIO_INEN - FIO_DIR)
//---------------------------------------
.section L1_code;

.global _GetCmdByte;
.type _GetCmdByte,STT_FUNC; 
.align 4; 

_GetCmdByte:
	link 40;
	[--sp] = r1;				// push regs to stack
	[--sp] = r2;	
	[--sp] = r3;	
	[--sp] = r4;
	[--sp] = r5;
	[--sp] = r6;
	[--sp] = r7;
	[--sp] = p0;	
	[--sp] = p1;	

	//-----------
	call PF_DataAsInput;		// set PFs as inputs
	//----------
	p0.h = 0x2000;				// pointer to Async mem. (ACSI control signals)
	p0.l = 0x0000;
	
	p1.h = hi(FIO_FLAG_D);		// pointer to PF (ACSI data)
	p1.l = lo(FIO_FLAG_D);
	
	r2 = (aCS | aRESET | aA1 | aRW) (Z);	// mask for aRESET, aCS, aA1, aRW
	r3 = aRESET (Z);			// result after masking: aRESET
	r4 = CARD_CHANGE (Z);		// when media change happened
	r6 = aRESET;
		
	nop;						// to avoid the 'read after write' warning
	nop;
	//----------

GetCmdByteLoop:
	r0 = w[p0];					// read ACSI control signals
	r1 = w[p1];					// read ACSI data
	
//------------
	r5 = r1 & r4;					// r5 = card change flag
	cc = r5 == r4;					// if card change flag is set
	
	r5 = 3 (Z);						// brStat = E_CARDCHANGE
	if cc jump GetCmdByteEnd;		// if media change
//------------
	r7 = r0 & r6;					// r7 = ST is in RESET state
	cc = r7 == 0;					// if the ST is in reset state
	
	r5 = 4 (Z);						// brStat = E_RESET
	if cc jump GetCmdByteEnd;		// then jump out :)
//------------
SkipmediaChange:
	r0 = r0 & r2;				// ACSI control signals & mask (aRESET, aCS, aA1, aRW)
	cc = r0 == r3;				// does the result match aRESET=H and rest L?
	
	if !cc jump GetCmdByteLoop;	// not matching, read again!
	//----------
	r2 = (aCS | aRESET) (Z);	// mask for aRESET, aCS -- removed aA1, aRW - just in case
	r3 = (aCS | aRESET) (Z);	// result - aRESET and aCS are H
	
GetCmdByteLoop2:

	r0 = w[p0];					// read ACSI control signals
	
	r0 = r0 & r2;				// ACSI control signals & mask (aRESET, aCS, aA1, aRW)
	cc = r0 == r3;				// does the result match aRESET=H, CS=H and rest L?
	
	if !cc jump GetCmdByteLoop2;	// not matching, read again!
	//----------
	r0 = r1 >> 8;				// shift the data down to the lower nibble
	r5 = 2 (Z);					// brStat = E_OK_A1
GetCmdByteEnd:
	//----------
	// store bridge status
	p0.h = hi(_brStat);			// get pointer to brStat in p0
	p0.l = lo(_brStat);	
	csync;						
	b[p0] = r5;					// store the value from r5
	//----------
	p1 = [sp++];				// pop regs from stack
	p0 = [sp++];
	r7 = [sp++];
	r6 = [sp++];
	r5 = [sp++];
	r4 = [sp++];
	r3 = [sp++];
	r2 = [sp++];
	r1 = [sp++];
	unlink;	
	rts;						// return 
_GetCmdByte.end:
//---------------------------------------
.global _PIO_write;
.type _PIO_write,STT_FUNC; 
.align 4; 

_PIO_write:
	link 0;
	[--sp] = r1;				// push regs to stack
	[--sp] = r2;	
	[--sp] = r3;	
	[--sp] = r4;	
	[--sp] = r5;	
	[--sp] = p0;	
	[--sp] = p1;	

	//----------
	// set bridge status OK (if error, it will set it to error at the end)
	p0.h = hi(_brStat);
	p0.l = lo(_brStat);	
	r0 = 0x0001 (Z);
	nop;
	nop;
	nop;
	b[p0] = r0;					// brStat = E_OK
	//----------
	// set up the registers
	p0.h = 0x2000;				// pointer to Async mem. (ACSI control signals)
	p0.l = 0x0000;
	
	p1.h = hi(FIO_FLAG_D);		// pointer to PF (ACSI data)
	p1.l = lo(FIO_FLAG_D);

	r2 = (aRESET | aCS | aRW) (Z);	// mask for aRESET, aCS, aRnW
	r3 = (aRESET) (Z);				// result after masking: aRESET
	r4 = (aIRQ) (Z);				// the aIRQ pin on PF

	r5.h = 0x00ff ;					// timeout
	r5.l = 0xffff ;				
	
	w[p1 + DC_ofs] = r4;			// aIRQ to L
	//----------
PIO_writeLoop:
	r0 = w[p0];					// read ACSI control signals
	r1 = w[p1];					// read ACSI data
	
	r5 += - 1;
	cc = r5 == 0;
	if cc jump PIO_writeTimeout;
	
	r0 = r0 & r2;				// ACSI control signals & mask (aRESET, aCS, aRW)
	cc = r0 == r3;				// does the result match aRESET H and rest L?
	
	if !cc jump PIO_writeLoop;	// not matching, read again!
	//----------
	w[p1 + DS_ofs] = r4;		// aIRQ to H
	//----------
	r5 = 0xffff (Z);			// timeout
	r2 = (aRESET | aCS) (Z);	// mask for aRESET, aCS -- aRnW was too fast on TT
	r3 = (aRESET | aCS) (Z);	// result after masking: aRESET and aCS

PIO_writeLoop2:
	r0 = w[p0];					// read ACSI control signals
	
	r5 += - 1;
	cc = r5 == 0;
	if cc jump PIO_writeTimeout;
	
	r0 = r0 & r2;				// ACSI control signals & mask (aRESET, aCS, aRW)
	cc = r0 == r3;				// does the result match aRESET H and aCS H
	
	if !cc jump PIO_writeLoop2;	// not matching, read again!
	//----------
	r0 = r1 >> 8;				// shift the data down to the lower nibble

PIO_writeEnd:
	p1 = [sp++];				
	p0 = [sp++];
	r5 = [sp++];
	r4 = [sp++];
	r3 = [sp++];
	r2 = [sp++];
	r1 = [sp++];
	unlink;	
	rts;						// return 
PIO_writeTimeout:
	w[p1 + DS_ofs] = r4;		// aIRQ to H 

	p0.h = hi(_brStat);
	p0.l = lo(_brStat);	
	r0 = 0x0000 (Z);
	csync;
	b[p0] = r0;					// brStat = E_TimeOut

	jump PIO_writeEnd;
_PIO_write.end:
//---------------------------------------
.global _PIO_read;
.type _PIO_read,STT_FUNC; 
.align 4; 

_PIO_read:
	link 0;
	[--sp] = r1;				// push regs to stack
	[--sp] = r2;	
	[--sp] = r3;	
	[--sp] = r4;	
	[--sp] = r5;	
	[--sp] = r7;	
	[--sp] = p0;	
	[--sp] = p1;	

	r7 = r0;					// save r0...
	//----------
	// ACSI data as OUTPUT
	call PF_DataAsOutput;		// set PFs as outputs
	//----------
	p0.h = 0x2000;				// pointer to Async mem. (ACSI control signals)
	p0.l = 0x0000;
	
	p1.h = hi(FIO_FLAG_D);		// pointer to PF (ACSI data)
	p1.l = lo(FIO_FLAG_D);
	//----------
	r2 = (aRESET | aCS | aRW) (Z);	// mask for aRESET, aCS, aRW
	r3 = (aRESET | aRW) (Z);		// result after masking: aRESET
	r4 = aIRQ (Z);					// IRQ bit

	r5.h = 0x00ff ;					// timeout
	r5.l = 0xffff ;				
	//----------
	r0 = 0x00fe (Z);			// remove-data-and-int-drq mask

	r1 = w[p1];					// read PFs
	r1 = r1 & r0;				// remove data from PF
	
	r7 = r7 << 8;				// shift data up
	r1 = r1 | r7;				// data and signals together
//	r1 = r1 | r4;				// IRQ to L (must put PF in H)
	
	w[p1] = r1;					// write new PF value with data and IRQ
	//----------
PIO_readLoop:
	r0 = w[p0];					// read ACSI control signals
	
	r5 += - 1;
	cc = r5 == 0;
	if cc jump PIO_readTimeout;
	
	r0 = r0 & r2;				// ACSI control signals & mask (aRESET, aCS)
	cc = r0 == r3;				// does the result match aRESET H and rest L?
	
	if !cc jump PIO_readLoop;	// not matching, read again!
	//----------
	w[p1 + DS_ofs] = r4;		// aIRQ to H
	r2 = (aRESET | aCS) (Z);	// mask for aRESET, aCS
	//----------
PIO_readLoop2:
	r0 = w[p0];					// read ACSI control signals
	
	r5 += - 1;
	cc = r5 == 0;
	if cc jump PIO_readTimeout;
	
	r0 = r0 & r2;				// ACSI control signals & mask (aRESET, aCS)
	cc = r0 == r2;				// does the result match aRESET H and CS H?
	
	if !cc jump PIO_readLoop2;	// not matching, read again!
	//----------
	r0 = 0xff00 (Z);
	w[p1 + DC_ofs] = r0;		// remove the data from PFs
	
	r0 = 0x0001 (Z);			// brStat = E_OK 
	
PIO_readEnd:
	call PF_DataAsInput;

	p1 = [sp++];
	p0 = [sp++];
	r7 = [sp++];
	r5 = [sp++];
	r4 = [sp++];
	r3 = [sp++];
	r2 = [sp++];
	r1 = [sp++];
	unlink;	
	rts;						// return 
PIO_readTimeout:
	w[p1 + DS_ofs] = r4;					// aIRQ to H

	p0.h = hi(_brStat);
	p0.l = lo(_brStat);	
	r0 = 0x0000 (Z);
	csync;
	b[p0] = r0;					// brStat = E_TimeOut

	jump PIO_readEnd;
_PIO_read.end:
//---------------------------------------
.global _PreDMA_read;
.type _PreDMA_read,STT_FUNC; 
.align 4; 
_PreDMA_read:
// set PFs which are used for ACSI data as OUTPUT and disable inputs
	link 0x00;

	[--sp] = r0;				// push regs to stack
	[--sp] = r1;
	[--sp] = p0;	
	//----------
	// set bridge status OK (if error, it will set it to error at the end)
	p0.h = hi(_brStat);
	p0.l = lo(_brStat);	
	r0 = 0x0001 (Z);
	nop;
	nop;
	nop;
	b[p0] = r0;					// brStat = E_OK
	//----------
	call PF_DataAsOutput;
	//----------

	p0 = [sp++];				// pop regs from stack
	r1 = [sp++];
	r0 = [sp++];
	
	unlink;
	
	rts;						// return 
_PreDMA_read.end:
//---------------------------------------
.global _PostDMA_read;
.type _PostDMA_read,STT_FUNC; 
.align 4; 
_PostDMA_read:
// set PFs which are used for ACSI data as INPUT and enable inputs
	link 0x00;
	
	call PF_DataAsInput;

	unlink;
	rts;						// return 
_PostDMA_read.end:
//---------------------------------------
.global _DMA_read;
.type _DMA_read,STT_FUNC; 
.align 4; 
_DMA_read:
	[--sp] = r1;				// push regs to stack
	[--sp] = r2;	
	[--sp] = r3;	
	[--sp] = r4;	
	[--sp] = r5;	
	[--sp] = r7;	
	[--sp] = p0;	
	[--sp] = p1;	

	r7 = r0;					// save r0...
	//----------

	p0.h = 0x2000;				// pointer to Async mem. (ACSI control signals)
	p0.l = 0x0000;
	
	p1.h = hi(FIO_FLAG_D);		// pointer to PF (ACSI data)
	p1.l = lo(FIO_FLAG_D);
	//----------
	r2 = (aRESET | aACK) (Z);	// mask for aRESET, aACK
	r3 = (aRESET) (Z);			// result after masking: aRESET
	r4 = (aDRQ) (Z);			// DRQ bit

	r5 = 0xffff (Z);			// timeout
	//----------
	r0 = 0x00fd (Z);			// remove-data-and-int-drq mask

	r1 = w[p1];					// read PFs
	r1 = r1 & r0;				// remove data from PF
	
	r7 = r7 << 8;				// shift data up
	r1 = r1 | r7;				// data and signals together

	w[p1] = r1;					// write new PF value with data and IRQ
	//----------
DMA_readLoop:
	r0 = w[p0];					// read ACSI control signals
	
	r5 += - 1;
	cc = r5 == 0;
	if cc jump DMA_readTimeout;
	
	r0 = r0 & r2;				// ACSI control signals & mask (aRESET, aCS)
	cc = r0 == r3;				// does the result match aRESET H and rest L?
	
	if !cc jump DMA_readLoop;	// not matching, read again!
	//----------
	w[p1 + DS_ofs] = r4;		// DRQ to H (must put PF into L)
	//----------
DMA_readLoop2:
	r0 = w[p0];					// read ACSI control signals
	
	r5 += - 1;
	cc = r5 == 0;
	if cc jump DMA_readTimeout;
	
	r0 = r0 & r2;				// ACSI control signals & mask (aRESET, aCS)
	cc = r0 == r2;				// does the result match aRESET H and ACK H?
	
	if !cc jump DMA_readLoop2;	// not matching, read again!
	//----------
	r0 = 0x0001 (Z);			// brStat = E_OK 
	
DMA_readEnd:
	p1 = [sp++];
	p0 = [sp++];
	r7 = [sp++];
	r5 = [sp++];
	r4 = [sp++];
	r3 = [sp++];
	r2 = [sp++];
	r1 = [sp++];
	rts;						// return 
	
DMA_readTimeout:
	w[p1 + DS_ofs] = r4;					// DRQ to H (must put PF into L)
	//----------
	// set bridge status OK (if error, it will set it to error at the end)
	p0.h = hi(_brStat);
	p0.l = lo(_brStat);	
	r0 = 0x0000 (Z);
	nop;
	nop;
	nop;
	b[p0] = r0;					// brStat = E_TimeOut
	//----------

	jump DMA_readEnd;
_DMA_read.end:
//---------------------------------------
.global _DMA_readFast;
.type _DMA_readFast,STT_FUNC; 
.align 4; 
_DMA_readFast:
	[--sp] = r1;				// push regs to stack
	[--sp] = r2;	
	[--sp] = r3;	
	[--sp] = r4;	
	[--sp] = r5;	
	[--sp] = r7;	
	[--sp] = p0;	
	[--sp] = p1;	

	r7 = r0;					// save r0...
	//----------

	p0.h = 0x2000;				// pointer to Async mem. (ACSI control signals)
	p0.l = 0x0000;
	
	p1.h = hi(FIO_FLAG_D);		// pointer to PF (ACSI data)
	p1.l = lo(FIO_FLAG_D);
	//----------
	r2 = (aRESET | aACK) (Z);	// mask for aRESET, aACK
	r3 = (aRESET) (Z);			// result after masking: aRESET
	r4 = (aDRQ) (Z);			// DRQ bit

	r5 = 0xffff (Z);			// timeout
	//----------
	r0 = 0x00fd (Z);			// remove-data-and-int-drq mask

	r1 = w[p1];					// read PFs
	r1 = r1 & r0;				// remove data from PF
	
	r7 = r7 << 8;				// shift data up
	r1 = r1 | r7;				// data and signals together

	w[p1] = r1;					// write new PF value with data and IRQ
	//----------
DMA_readLoopF:
	r0 = w[p0];					// read ACSI control signals
	
	r5 += - 1;
	cc = r5 == 0;
	if cc jump DMA_readTimeoutF;
	
	r0 = r0 & r2;				// ACSI control signals & mask (aRESET, aCS)
	cc = r0 == r3;				// does the result match aRESET H and rest L?
	
	if !cc jump DMA_readLoopF;	// not matching, read again!
	//----------
	w[p1 + DS_ofs] = r4;		// DRQ to H
	r0 = 0x0001 (Z);			// brStat = E_OK 
	//----------

DMA_readEndF:
	p1 = [sp++];
	p0 = [sp++];
	r7 = [sp++];
	r5 = [sp++];
	r4 = [sp++];
	r3 = [sp++];
	r2 = [sp++];
	r1 = [sp++];
	rts;						// return 
	
DMA_readTimeoutF:
	w[p1 + DS_ofs] = r4;		// DRQ to H
	//----------
	// set bridge status OK (if error, it will set it to error at the end)
	p0.h = hi(_brStat);
	p0.l = lo(_brStat);	
	r0 = 0x0000 (Z);
	nop;
	nop;
	nop;
	b[p0] = r0;					// brStat = E_TimeOut
	//----------

	jump DMA_readEndF;
_DMA_readFast.end:
//---------------------------------------
.global _PreDMA_write;
.type _PreDMA_write,STT_FUNC; 
.align 4; 
_PreDMA_write:
	link 0;
	[--sp] = p0;				// push regs to stack
	[--sp] = r0;	

	//----------
	// set bridge status OK (if error, it will set it to error at the end)
	p0.h = hi(_brStat);
	p0.l = lo(_brStat);	
	r0 = 0x0001 (Z);
	nop;
	nop;
	nop;
	b[p0] = r0;					// brStat = E_OK
	//----------
	call PF_DataAsInput;		// set PFs as inputs
	
	r0 = [sp++];				// pop regs from stack
	p0 = [sp++];

	unlink;
	rts;
_PreDMA_write.end:
//---------------------------------------

.global _PostDMA_write;
.type _PostDMA_write,STT_FUNC; 
.align 4; 
_PostDMA_write:
	link 0x00;
	
	call PF_DataAsInput;
	
	unlink;
	rts;						
_PostDMA_write.end:
//---------------------------------------
.global _DMA_write;
.type _DMA_write,STT_FUNC; 
.align 4; 
_DMA_write:
	[--sp] = r1;				// push regs to stack
	[--sp] = r2;	
	[--sp] = r3;	
	[--sp] = r4;	
	[--sp] = r5;	
	[--sp] = p0;	
	[--sp] = p1;	

	p0.h = 0x2000;				// pointer to Async mem. (ACSI control signals)
	p0.l = 0x0000;
	
	p1.h = hi(FIO_FLAG_D);		// pointer to PF (ACSI data)
	p1.l = lo(FIO_FLAG_D);

	r2 = (aRESET | aACK) (Z);	// mask for aRESET, aACK
	r3 = (aRESET) (Z);			// result after masking: aRESET
	r4 = (aDRQ) (Z);			// the aDRQ pin on PF

	r5 = 0xffff (Z);			// timeout
	
	w[p1 + DC_ofs] = r4;		// aDRQ to L 
	//----------
DMA_writeLoop:
	r0 = w[p0];					// read ACSI control signals
	r1 = w[p1];					// read ACSI data
	
	r5 += - 1;
	cc = r5 == 0;
	if cc jump DMA_writeTimeout;
	
	r0 = r0 & r2;				// ACSI control signals & mask (aRESET, aACK)
	cc = r0 == r3;				// does the result match aRESET H and rest L?
	
	if !cc jump DMA_writeLoop;	// not matching, read again!
	//----------
	w[p1 + DS_ofs] = r4;		// aDRQ to H

	r0 = r1 >> 8;				// shift the data down to the lower nibble
//--------------
DMA_writeLoop2:
	r1 = w[p0];					// read ACSI control signals
	
	r5 += - 1;
	cc = r5 == 0;
	if cc jump DMA_writeTimeout;
	
	r1 = r1 & r2;				// ACSI control signals & mask (aRESET, aACK)
	cc = r1 == r2;				// does the result match (aRESET, aACK) as H
	
	if !cc jump DMA_writeLoop2;	// not matching, read again!
	//----------

DMA_writeEnd:
	p1 = [sp++];				
	p0 = [sp++];
	r5 = [sp++];
	r4 = [sp++];
	r3 = [sp++];
	r2 = [sp++];
	r1 = [sp++];
	rts;						// return 
DMA_writeTimeout:
	w[p1 + DS_ofs] = r4;		// aDRQ to H (must put PF into L)

	p0.h = hi(_brStat);
	p0.l = lo(_brStat);	
	r0 = 0x0000 (Z);
	nop;
	nop;
	nop;
	b[p0] = r0;					// brStat = E_TimeOut

	jump DMA_writeEnd;
_DMA_write.end:
//---------------------------------------
.global _DMA_writeFast;
.type _DMA_writeFast,STT_FUNC; 
.align 4; 
_DMA_writeFast:
	[--sp] = r1;				// push regs to stack
	[--sp] = r2;	
	[--sp] = r3;	
	[--sp] = r4;	
	[--sp] = r5;	
	[--sp] = p0;	
	[--sp] = p1;	

	p0.h = 0x2000;				// pointer to Async mem. (ACSI control signals)
	p0.l = 0x0000;
	
	p1.h = hi(FIO_FLAG_D);		// pointer to PF (ACSI data)
	p1.l = lo(FIO_FLAG_D);

	r2 = (aRESET | aACK) (Z);	// mask for aRESET, aACK
	r3 = (aRESET) (Z);			// result after masking: aRESET
	r4 = (aDRQ) (Z);			// the aDRQ pin on PF

	r5 = 0xffff (Z);			// timeout
	
	//--------------
// 1st - wait until ACK is high
DMA_writeFastLoop2:
	r1 = w[p0];					// read ACSI control signals
	
	r5 += - 1;
	cc = r5 == 0;
	if cc jump DMA_writeFastTimeout;
	
	r1 = r1 & r2;				// ACSI control signals & mask (aRESET, aACK)
	cc = r1 == r2;				// does the result match (aRESET, aACK) as H
	
	if !cc jump DMA_writeFastLoop2;	// not matching, read again!
	//--------------
// 2nd - set DRQ to L
	w[p1 + DC_ofs] = r4;		// aDRQ to L 
	//----------
DMA_writeFastLoop:
	r0 = w[p0];					// read ACSI control signals
	r1 = w[p1];					// read ACSI data
	
	r5 += - 1;
	cc = r5 == 0;
	if cc jump DMA_writeFastTimeout;
	
	r0 = r0 & r2;				// ACSI control signals & mask (aRESET, aACK)
	cc = r0 == r3;				// does the result match aRESET H and rest L?
	
	if !cc jump DMA_writeFastLoop;	// not matching, read again!
	//----------
	w[p1 + DS_ofs] = r4;		// aDRQ to H

	r0 = r1 >> 8;				// shift the data down to the lower nibble
	//----------

DMA_writeFastEnd:
	p1 = [sp++];				
	p0 = [sp++];
	r5 = [sp++];
	r4 = [sp++];
	r3 = [sp++];
	r2 = [sp++];
	r1 = [sp++];
	rts;						// return 
DMA_writeFastTimeout:
	w[p1 + DS_ofs] = r4;		// aDRQ to H 

	p0.h = hi(_brStat);
	p0.l = lo(_brStat);	
	r0 = 0x0000 (Z);
	nop;
	nop;
	nop;
	b[p0] = r0;					// brStat = E_TimeOut

	jump DMA_writeFastEnd;
_DMA_writeFast.end:
//---------------------------------------
.global _DMA_writeFastPost;
.type _DMA_writeFastPost,STT_FUNC; 
.align 4; 
_DMA_writeFastPost:
	[--sp] = r1;				// push regs to stack
	[--sp] = r2;	
	[--sp] = r3;	
	[--sp] = r5;	
	[--sp] = p0;	

	p0.h = 0x2000;				// pointer to Async mem. (ACSI control signals)
	p0.l = 0x0000;
	
	r2 = (aRESET | aACK) (Z);	// mask for aRESET, aACK
	r3 = (aRESET) (Z);			// result after masking: aRESET

	r5 = 0xffff (Z);			// timeout
	
//--------------
DMA_writeFastPostLoop2:
	r1 = w[p0];					// read ACSI control signals
	
	r5 += - 1;
	cc = r5 == 0;
	if cc jump DMA_writeFastPostTimeout;
	
	r1 = r1 & r2;				// ACSI control signals & mask (aRESET, aACK)
	cc = r1 == r2;				// does the result match (aRESET, aACK) as H
	
	if !cc jump DMA_writeFastPostLoop2;	// not matching, read again!
	//----------

DMA_writeFastPostEnd:
	p0 = [sp++];
	r5 = [sp++];
	r3 = [sp++];
	r2 = [sp++];
	r1 = [sp++];
	rts;						// return 
DMA_writeFastPostTimeout:
	p0.h = hi(_brStat);
	p0.l = lo(_brStat);	
	r0 = 0x0000 (Z);
	nop;
	nop;
	nop;
	b[p0] = r0;					// brStat = E_TimeOut

	jump DMA_writeFastPostEnd;
_DMA_writeFastPost.end:
//---------------------------------------
.global PF_DataAsOutput;
.type PF_DataAsOutput,STT_FUNC; 
.align 4; 

PF_DataAsOutput:
	[--sp] = r0;						// push regs to stack
	[--sp] = p0;
	
	//----------
	p0.h = hi(FIO_DIR);			
	p0.l = lo(FIO_DIR);

	nop;
	nop;
	nop;
	
	r0 = PF_OUTPUTS | aDATA_MASK (Z);
	w[p0] = r0;							// FIO_DIR: store the new PF directions
	//----------
	r0 = CARD_CHANGE (Z);				// FIO_INEN: disable inputs for the ACSI data pins
	w[p0 + DIR_INEN_ofs] = r0;
	//----------
	p0 = [sp++];
	r0 = [sp++];						
	
	rts;						
PF_DataAsOutput.end:
//---------------------------------------
.global PF_DataAsInput;
.type PF_DataAsInput,STT_FUNC; 
.align 4; 

PF_DataAsInput:
	[--sp] = r0;						// push regs to stack
	[--sp] = p0;
	
	//----------
	p0.h = hi(FIO_DIR);			
	p0.l = lo(FIO_DIR);

	nop;
	nop;
	nop;
	
	r0 = PF_OUTPUTS (Z);
	w[p0] = r0;								// store the new PF directions
	//----------
	r0 = (aDATA_MASK | CARD_CHANGE) (Z);	// enable inputs for the ACSI data pins
	w[p0 + DIR_INEN_ofs] = r0;
	//----------
	p0 = [sp++];
	r0 = [sp++];						
	
	rts;						
PF_DataAsInput.end:
//---------------------------------------

