//---------------------------------------
#include <defBF531.h>
#include "pin_defines.h"

//---------------------------------------
.section L1_code;

.global _spiTransferByteAsm;
.type _spiTransferByteAsm,STT_FUNC; 
.align 4; 

_spiTransferByteAsm:
	[--sp] = r1;				// push regs to stack
	[--sp] = p0;	
	[--sp] = p1;	
	[--sp] = p2;	

	p0.h = hi(SPI_STAT);
	p0.l = lo(SPI_STAT);
	
	p1.h = hi(SPI_TDBR);
	p1.l = lo(SPI_TDBR);
	
	p2.h = hi(SPI_RDBR);
	p2.l = lo(SPI_RDBR);
//-------
WaitForTXS0:
	r1 = w[p0];					// read SPI_STAT

	cc = BITTST(r1, 3) ;		// get only TXS bit (SPI_TDBR Data Buffer Status)
	if cc jump WaitForTXS0;		// if TXS is not 0, wait

	w[p1] = r0.l;				// write the required data into SPI_TDBR
//-------
WaitForRXS1:
	r1 = w[p0];					// read SPI_STAT

	cc = BITTST(r1, 5) ;		// get only RXS bit (RX Data Buffer Status)
	if !cc jump WaitForRXS1;	// if RXS is not 1, wait

	r0.l = w[p2];				// read data from SPI_RDBR
//-------
			
	p2 = [sp++];				// pop regs from stack
	p1 = [sp++];
	p0 = [sp++];
	r1 = [sp++];
	rts;						// return 
_spiTransferByteAsm.end:
//---------------------------------------
.global _spiSendByteAsm;
.type _spiSendByteAsm,STT_FUNC; 
.align 4; 

_spiSendByteAsm:
	[--sp] = r1;				// push regs to stack
	[--sp] = p0;	
	[--sp] = p1;	
	[--sp] = p2;	

	p0.h = hi(SPI_STAT);
	p0.l = lo(SPI_STAT);
	
	p2.h = hi(SPI_RDBR);
	p2.l = lo(SPI_RDBR);

	p1.h = hi(SPI_TDBR);
	p1.l = lo(SPI_TDBR);
	//-------
	r1 = w[p0];					// read SPI_STAT

	cc = BITTST(r1, 5);			// get only RXS bit (RX Data Buffer Status)
	if !cc jump WaitForTXSwait;	// if RXS is not 1, skip reading

	r1.l = w[p2];				// read data from SPI_RDBR, but don't store
//-------
WaitForTXSwait:
	r1 = w[p0];					// read SPI_STAT

	cc = BITTST(r1, 3);			// get only TXS bit (SPI_TDBR Data Buffer Status)
	if cc jump WaitForTXSwait;	// if TXS is not 0, wait

	w[p1] = r0.l;				// write the required data into SPI_TDBR
//-------
			
	p2 = [sp++];				// pop regs from stack
	p1 = [sp++];
	p0 = [sp++];
	r1 = [sp++];
	rts;						// return 
_spiSendByteAsm.end:
//---------------------------------------
.extern _DMA_read;

.global _MMC_ReadAsm;
.type _MMC_ReadAsm,STT_FUNC; 
.align 4; 

_MMC_ReadAsm:
	link 0x00;
	[--sp] = r0;				
	[--sp] = r1;				
	[--sp] = r2;				
	[--sp] = r3;				
	[--sp] = p0;	
	[--sp] = p1;	
	[--sp] = p2;	

	p0.h = hi(SPI_STAT);
	p0.l = lo(SPI_STAT);
	
	p1.h = hi(SPI_TDBR);
	p1.l = lo(SPI_TDBR);
	
	p2.h = hi(SPI_RDBR);
	p2.l = lo(SPI_RDBR);
	
	r2.l = 0xffff;
	r3 = 0x0200 (Z);
//-------
	call _spiTransferByteAsm;
//-------
MWaitForTXS0:
	r1 = w[p0];					// read SPI_STAT

	cc = BITTST(r1, 3) ;		// get only TXS bit (SPI_TDBR Data Buffer Status)
	if cc jump MWaitForTXS0;	// if TXS is not 0, wait
	
	w[p1] = r2.l;				// write the required data into SPI_TDBR
//-------
	call _DMA_read;
//-------
MWaitForRXS1:
	r1 = w[p0];					// read SPI_STAT

	cc = BITTST(r1, 5) ;		// get only RXS bit (RX Data Buffer Status)
	if !cc jump MWaitForRXS1;	// if RXS is not 1, wait

	r0.l = w[p2];				// read data from SPI_RDBR
//-------
	r3 += -1;
	cc = r3 == 0;
	
	if !cc jump MWaitForTXS0;			

	p2 = [sp++];				// pop regs from stack
	p1 = [sp++];
	p0 = [sp++];
	r3 = [sp++];
	r2 = [sp++];
	r1 = [sp++];
	r0 = [sp++];
	unlink;
	rts;						// return 
_MMC_ReadAsm.end:
//---------------------------------------
.global _spiReceiveByteAsm;
.type _spiReceiveByteAsm,STT_FUNC; 
.align 4; 

_spiReceiveByteAsm:
	[--sp] = r1;	
	[--sp] = p0;	
	[--sp] = p1;	
	[--sp] = p2;	

	p0.h = hi(SPI_STAT);
	p0.l = lo(SPI_STAT);
	
	p1.h = hi(SPI_RDBR);
	p1.l = lo(SPI_RDBR);
	
	p2.h = hi(SPI_CTL);
	p2.l = lo(SPI_CTL);
//-------
WaitForRXS1R:
	r1 = w[p0] (Z);				// read SPI_STAT

	cc = BITTST(r1, 5) ;		// get only RXS bit (RX Data Buffer Status)
	if !cc jump WaitForRXS1R;	// if RXS is not 1, wait
//-------
	r1 = 511 (Z);
	cc = r0 == r1;				// if we're receiving 511 byte, dissable SPI
	if !cc jump spiReceiveByteFinish;
	
	r1 = 0x0000 (Z);				
	w[p2] = r1;					// dissable SPI
//-------
spiReceiveByteFinish:
	r0 = w[p1] (Z);				// read data from SPI_RDBR
//-------
	p2 = [sp++];
	p1 = [sp++];
	p0 = [sp++];
	r1 = [sp++];
	rts;						// return 
_spiReceiveByteAsm.end:
//---------------------------------------
.extern _DMA_readFast;

.global _SPIreadSectorAsm;
.type _SPIreadSectorAsm,STT_FUNC; 
.align 4; 

_SPIreadSectorAsm:
	link 0x00;
	[--sp] = r1;	
	[--sp] = r2;	
	[--sp] = p0;	
	[--sp] = p1;	
	[--sp] = p2;	
	[--sp] = p3;	

	p0.h = hi(SPI_STAT);
	p0.l = lo(SPI_STAT);
	
	p1.h = hi(SPI_RDBR);
	p1.l = lo(SPI_RDBR);
	
	p2.h = hi(SPI_CTL);
	p2.l = lo(SPI_CTL);

	p3.h = hi(SPI_TDBR);
	p3.l = lo(SPI_TDBR);
	
	r1 = 511 (Z);				// number of bytes
	r2 = 0xff (Z);				// dummy byte to send
//-------
// for 511 bytes
WaitForRXS1RA:
	r0 = w[p0] (Z);				// read SPI_STAT

	cc = BITTST(r0, 5) ;		// get only RXS bit (RX Data Buffer Status)
	if !cc jump WaitForRXS1RA;	// if RXS is not 1, wait

	w[p3] = r2;					// senf always 0xff
	r0 = w[p1] (Z);				// read data from SPI_RDBR
	//------------------
	call _DMA_readFast;

	cc = r0 == 1;				// is bridge status E_OK?
	if !cc jump SPIreadSectorAsmError;

	r1 += -1;
	cc = r1 == 0;
	if !cc jump WaitForRXS1RA;
///////////////////
// for the last byte
WaitForRXS1RA2:
	r0 = w[p0] (Z);				// read SPI_STAT

	cc = BITTST(r0, 5) ;		// get only RXS bit (RX Data Buffer Status)
	if !cc jump WaitForRXS1RA2;	// if RXS is not 1, wait
	//-----
	r0 = 0x0000 (Z);				
	w[p2] = r0;					// dissable SPI
	//-----
	r0 = w[p1] (Z);				// read data from SPI_RDBR
	//------------------
	call _DMA_readFast;

	r0 = 0;						// everything OK ;) (will send error according to brStat if needed)
//-------
SPIreadSectorAsmFinish:

	p3 = [sp++];
	p2 = [sp++];
	p1 = [sp++];
	p0 = [sp++];
	r2 = [sp++];
	r1 = [sp++];
	unlink;
	rts;						// return 
	
SPIreadSectorAsmError:	
	r0 = r1;
	jump SPIreadSectorAsmFinish;

_SPIreadSectorAsm.end:
//---------------------------------------
.extern _DMA_writeFast;
.extern _brStat;

.global _SPIwriteSectorAsm;
.type _SPIwriteSectorAsm,STT_FUNC; 
.align 4; 

_SPIwriteSectorAsm:
	link 0x00;
	[--sp] = r1;	
	[--sp] = r2;	
	[--sp] = r3;	
	[--sp] = p0;	
	[--sp] = p1;	
	[--sp] = p2;	
	[--sp] = p3;	
	[--sp] = p4;	

	p0.h = hi(SPI_STAT);
	p0.l = lo(SPI_STAT);
	
	p1.h = hi(SPI_RDBR);
	p1.l = lo(SPI_RDBR);
	
	p2.h = hi(SPI_CTL);
	p2.l = lo(SPI_CTL);

	p3.h = hi(SPI_TDBR);
	p3.l = lo(SPI_TDBR);

	p4.h = hi(_brStat);
	p4.l = lo(_brStat);	
	
	r1 = 512 (Z);				// number of bytes
//-------
// for 512 bytes
WriteSectorLoop:
	call _DMA_writeFast;					// get byte from ST

	r3 = b[p4];								// read bsStat
	cc = r3 == 0;							// compate to TimeOut
	if cc jump SPIwriteSectorAsmError;		// if timeout, error!
	//------------------
	r3 = w[p0];						// read SPI_STAT

	cc = BITTST(r3, 5);				// get only RXS bit (RX Data Buffer Status)
	if !cc jump WaitForTXSwaitX;	// if RXS is not 1, skip reading

	r3.l = w[p1];					// read data from SPI_RDBR, but don't store
//-------
WaitForTXSwaitX:
	r3 = w[p0];					// read SPI_STAT

	cc = BITTST(r3, 3);			// get only TXS bit (SPI_TDBR Data Buffer Status)
	if cc jump WaitForTXSwaitX;	// if TXS is not 0, wait

	w[p3] = r0.l;				// write the required data into SPI_TDBR
	//------------------

	r1 += -1;
	cc = r1 == 0;
	if !cc jump WriteSectorLoop;
///////////////////
	r0 = 0;						// everything OK ;) (will send error according to brStat if needed)
//-------
SPIwriteSectorAsmFinish:

	p4 = [sp++];
	p3 = [sp++];
	p2 = [sp++];
	p1 = [sp++];
	p0 = [sp++];
	r3 = [sp++];
	r2 = [sp++];
	r1 = [sp++];
	unlink;
	rts;						// return 
	
SPIwriteSectorAsmError:	
	r0 = r1;
	jump SPIwriteSectorAsmFinish;

_SPIwriteSectorAsm.end:
//---------------------------------------
