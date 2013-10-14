/*****************************************************************************
 * US_init.asm
 *****************************************************************************/

#include <defBF531.h>
 
.SECTION program;
.ALIGN 4;

	// E:\bf\US_init\Debug\US_init.dxe

	// R3 is address in flash: 0x0000048E (may be different because of the size of init block)

	// save registers onto stack
	[--SP] = ASTAT;		//save registers onto stack 
	[--SP] = RETS;
	[--SP] = (R7:4);
	[--SP] = (P5:0);
	
////////////////////////
// switch PLL frequency

	p5.h = HI(SIC_IWR);			// PLL WAKEUP enable
	p5.l = LO(SIC_IWR);
	r7 = 0x0001 (Z);
	csync;
	[p5] = r7;
	//-----------
	p5.h = HI(PLL_LOCKCNT);		// LOCKCNT = 512
	p5.l = LO(PLL_LOCKCNT);
	r7 = 0x0200 (Z);
	csync;
	w[p5] = r7;
	//-----------
	p5.h = HI(PLL_DIV);			// CCLK = VCO, SCLK = VCO/3
	p5.l = LO(PLL_DIV);
	r7 = 0x0003 (Z);
	csync;
	w[p5] = r7;
	//-----------
	p5.h = HI(PLL_CTL);			// VCO = CLKIN * 20 (20 * 20 MHz)
	p5.l = LO(PLL_CTL);
//	r7 = 0x2800 (Z);			// MSEL = 20 (20 * 20 MHz = 400 MHz)
	r7 = 0x3200 (Z);			// MSEL = 25 (25 * 16 MHz = 400 MHz)
//	r7 = 0x5000 (Z);			// MSEL = 40 (40 * 10 MHz = 400 MHz)
	csync;
	//-----------
	cli r6;						// dissable interrupts
	w[p5] = r7;					// write the new PLL_CTL
	
	idle;						// go IDLE and wait for PLL WAKEUP
	sti r6;	
	
// end of frequency switch
////////////////////////
// setup UART

	// set 57600/115200,8,N,1 with SCLK = 133 MHz
	
	p5.h = HI(UART_GCTL);
	p5.l = LO(UART_GCTL);
	r7 = 0x0000 (Z);
	csync;
	w[p5] = r7;
	//-----------
	p5.h = HI(UART_LCR);	
	p5.l = LO(UART_LCR);
	r7 = 0x0083 (Z);
	csync;
	w[p5] = r7;
	//-----------
	p5.h = HI(UART_DLH);	
	p5.l = LO(UART_DLH);
	r7 = 0x0000 (Z);
	csync;
	w[p5] = r7;
	//-----------
	p5.h = HI(UART_DLL);	
	p5.l = LO(UART_DLL);
	r7 = 0x0048 (Z);
	csync;
	w[p5] = r7;
	//-----------
	p5.h = HI(UART_GCTL);	
	p5.l = LO(UART_GCTL);
	r7 = 0x0001 (Z);
	csync;
	w[p5] = r7;
	//-----------
	p5.h = HI(UART_LCR);	
	p5.l = LO(UART_LCR);
	r7 = 0x0003 (Z);
	csync;
	w[p5] = r7;
	//-----------
	
// end of UART setup 
////////////////////////
/*
DeadLoop:

	r6.l = 65;		
	call putc; 	
	jump DeadLoop;
*/
////////////////////////
// write out the string 'Boot:'
	r6.l = 10;		// \n
	call putc; 	
	r6.l = 13;		// \r
	call putc; 	

	r6.l = 66;		// B
	call putc; 	

	r6.l = 111;		// o
	call putc; 	

	r6.l = 111;		// o
	call putc; 	

	r6.l = 116;		// t
	call putc; 	

	r6.l = 58;		// :
	call putc; 	
/////////////////////////
	// setup EBIU
	//-----------
	p5.h = HI(EBIU_AMGCTL);	
	p5.l = LO(EBIU_AMGCTL);
	r7 = 0x00fe (Z);
	csync;
	w[p5] = r7;						// CLKOUT disable, all banks (0,1,2,3) enabled 
	//-----------
	p5.h = HI(EBIU_AMBCTL0);	
	p5.l = LO(EBIU_AMBCTL0);
	r7.h = 0x1114;
	r7.l = 0x1114;
	csync;
	[p5] = r7;
	//-----------
	p5.h = HI(EBIU_AMBCTL1);	
	p5.l = LO(EBIU_AMBCTL1);
	r7.h = 0x1114;
	r7.l = 0x1114;
	csync;
	[p5] = r7;
	//-----------
	
	// Check BOOT BASE FW jumper
	p5.h = 0x2000;		// pointer to Async mem. (control signals)
	p5.l = 0x0000;
	csync;
	r7 = w[p5];

	cc = bittst(r7, 7);			// test BOOT BASE pin
	if !cc jump JustBootBase;	// if should boot base FW

/////////////////////////
/*
	// write out R3
	r6 = r3;
	call putHexD;
*/
	//------------------
	// read which FW # we should boot
	call spisetup;				// setup SPI 

	r6 = 2020 (Z);				// read config page
	call readpage;

	p5.h = 0xFF80;				
	p5.l = 0x6000;
	csync;
	
	r6 = b[p5++] (Z);			// get just 0th byte (# of FW we should boot)
	//------------------
	// check if we should rather boot base FW
	cc = r6 == 0;				// if should boot base FW
	if cc jump JustBootBase;
	
	cc = r6 < 5 (IU);			// FW # out of range?
	if cc jump FWnoIsOK;
	
	jump JustBootBase;
FWnoIsOK:
	//------------------
	// OK, now we know that we should not boot base FW and the FW # is OK
	call putHex; 	// write out the FW #
	
	//--------
	cc = r6 == 0;
	if !cc jump FWNot0;
	r5 = 0 (Z);
	jump BootIt;
FWNot0:
	//--------
	cc = r6 == 1;
	if !cc jump FWNot1;
	r5 = 400 (Z);
	jump BootIt;	
FWNot1:
	//--------
	cc = r6 == 2;
	if !cc jump FWNot2;
	r5 = 800 (Z);
	jump BootIt;	
FWNot2:
	//--------
	cc = r6 == 3;
	if !cc jump FWNot3;
	r5 = 1200 (Z);
	jump BootIt;	
FWNot3:
	//--------
	cc = r6 < 5 (IU);
	if !cc jump FWNot4;
	r5 = 1600 (Z);
	jump BootIt;	
FWNot4:

	jump JustBootBase;
	//--------
BootIt:			
	
	r6.l = 32;		// [space]
	call putc; 	

	r6 = r5;
	call putHexD;

	r6 = 0xC800 (Z);		// 50 kB
	cc = r3 < r6;			// is the current booting address below 50 kB?
	if !cc jump WeAreDone;	// if we're booting from NOT BASE, just continue
	
							// if we're booting from BASE, calculate new address
	r3 = r5 << 8;			// address = page * 256;
	
////////////////////////	
WeAreDone:
//	call UARTwait;

	p5.h = HI(SIC_IWR);			// wake up from ALL enable
	p5.l = LO(SIC_IWR);
	r7.h = 0xffff;
	r7.l = 0xffff;
	csync;
	[p5] = r7;
	//-----------
	// restore registers from stack
	(P5:0) = [SP++];  			// Restore Regs
	(R7:4) = [SP++];
	RETS = [SP++];
	ASTAT = [SP++];

	RTS;
///////////////////////////////////////////////////
JustBootBase:
	
	r6.l = 66;		// B
	call putc; 	

	r6.l = 97;		// a
	call putc; 	

	r6.l = 115;		// s
	call putc; 	

	r6.l = 101;		// e
	call putc; 	
	//------------
	r6 = 0xC800 (Z);		// 50 kB
	cc = r3 < r6;			// is the current booting address below 50 kB?
	if cc jump WeAreDone;	// if we're booting BASE, just finish
	
	r3 = 0x0000 (Z);		// if we're not booting BASE, change address to start of flash (BASE)
	jump WeAreDone;
///////////////////////////////////////////////////
	.global putHexD;
	.type putHexD,STT_FUNC; 
	.align 4; 

putHexD:
	[--SP] = ASTAT; 
	[--SP] = RETS;
	[--SP] = r7;
	[--SP] = r6;
	[--SP] = r5;
	[--SP] = r4;
	[--SP] = p5;
	//----------
	r7 = 0x00ff (Z);
	r5 = r6;
	
	r6 = r5 >> 24;
	r6 = r6 & r7;
	call putHex;
	
	r6 = r5 >> 16;
	r6 = r6 & r7;
	call putHex;

	r6 = r5 >>  8;
	r6 = r6 & r7;
	call putHex;

	r6 = r5;
	r6 = r6 & r7;
	call putHex;
	
	//----------
	p5 = [SP++];
	r4 = [SP++];
	r5 = [SP++];
	r6 = [SP++];
	r7 = [SP++];
	RETS = [SP++];
	ASTAT = [SP++];

	rts;						// return 

putHexD.end:
///////////////////////////////////////////////////
	.global putD;
	.type putD,STT_FUNC; 
	.align 4; 

putD:
	[--SP] = ASTAT; 
	[--SP] = RETS;
	[--SP] = r7;
	[--SP] = r6;
	[--SP] = r5;
	[--SP] = r4;
	[--SP] = p5;
	//----------
	r7 = 0x00ff (Z);
	r5 = r6;
	
	r6 = r5 >> 24;
	r6 = r6 & r7;
	call putc;
	
	r6 = r5 >> 16;
	r6 = r6 & r7;
	call putc;

	r6 = r5 >>  8;
	r6 = r6 & r7;
	call putc;

	r6 = r5;
	r6 = r6 & r7;
	call putc;
	
	//----------
	p5 = [SP++];
	r4 = [SP++];
	r5 = [SP++];
	r6 = [SP++];
	r7 = [SP++];
	RETS = [SP++];
	ASTAT = [SP++];

	rts;						// return 

putD.end:
///////////////////////////////////////////////////
	.global putHex;
	.type putHex,STT_FUNC; 
	.align 4; 

putHex:
	[--SP] = ASTAT; 
	[--SP] = RETS;
	[--SP] = r7;
	[--SP] = r6;
	[--SP] = r5;
	[--SP] = r4;
	[--SP] = p5;
	//----------
	r7 = 0x00f0 (Z);
	r5 = r6 & r7;
	r5 = r5 >> 4;
	
	r4 = 0x000a (Z);
	cc = r5 < r4;
	if cc jump putBelow1;	

	r5 += 55;
	jump putNext1;
	
putBelow1:
	r5 += 48;
putNext1:

	r7 = r6;
	r6 = r5;
	call putc;
	r6 = r7;
	
	//----------
	r7 = 0x000f (Z);
	r5 = r6 & r7;
	
	r4 = 0x000a (Z);
	cc = r5 < r4;
	if cc jump putBelow2;	

	r5 += 55;
	jump putNext2;
	
putBelow2:
	r5 += 48;
putNext2:

	r6 = r5;
	call putc;
	
	//----------
	p5 = [SP++];
	r4 = [SP++];
	r5 = [SP++];
	r6 = [SP++];
	r7 = [SP++];
	RETS = [SP++];
	ASTAT = [SP++];

	rts;						// return 

putHex.end:
///////////////////////////////////////////////////
	.global putc;
	.type putc,STT_FUNC; 
	.align 4; 

putc:
	[--SP] = ASTAT; 
	[--SP] = RETS;
	[--SP] = r7;
	[--SP] = p5;

	p5.h = HI(UART_LSR);	
	p5.l = LO(UART_LSR);
	csync;
	
putcwait:
	r7 = w[p5];	
	cc = bittst(r7, 6);

	if !cc jump putcwait;	
	//---

	p5.h = HI(UART_THR);	
	p5.l = LO(UART_THR);
	csync;
	w[p5] = r6.l;

	//----------
	p5 = [SP++];
	r7 = [SP++];
	RETS = [SP++];
	ASTAT = [SP++];

	rts;						// return 
putc.end:
///////////////////////////////////////////////////
	.global UARTwait;
	.type UARTwait,STT_FUNC; 
	.align 4; 

UARTwait:
	[--SP] = ASTAT; 
	[--SP] = RETS;
	[--SP] = r7;
	[--SP] = p5;

	p5.h = HI(UART_LSR);	
	p5.l = LO(UART_LSR);
	csync;
	
UARTwaitLoop:
	r7 = w[p5];	
	cc = bittst(r7, 6);

	if !cc jump UARTwaitLoop;	
	//---
	p5 = [SP++];
	r7 = [SP++];
	RETS = [SP++];
	ASTAT = [SP++];

	rts;						// return 
UARTwait.end:
///////////////////////////////////////////////////
	.global readpage;
	.type readpage,STT_FUNC; 
	.align 4; 

readpage:
	[--SP] = ASTAT; 
	[--SP] = RETS;
	[--SP] = r7;
	[--SP] = r6;
	[--SP] = r5;
	[--SP] = p5;
	//----------
	r7 = 0x07ff (Z);
	r5 = r6 & r7;
	r5 = r5 << 8;							// prepare page address
	
	p5.h = HI(FIO_FLAG_C);	
	p5.l = LO(FIO_FLAG_C);
	r7.l = 0x0004;				
	csync;
	w[p5] = r7.l;							// PF2 to L
	//----------
	r6 = 0x00d2 (Z);						// 'Main Memory Page Read' command
	call spitx;

	r7 = 0x00ff (Z);
	
	r6 = r5 >> 16;							// ((page >> 16) & 0xff)
	r6 = r6 & r7;
	call spitx;
	 
	r6 = r5 >> 8;							// ((page >>  8) & 0xff)
	r6 = r6 & r7;
	call spitx;

	r6 = r5;								// ((page      ) & 0xff)
	r6 = r6 & r7;
	call spitx;

	//---------
	r6 = 0 (Z);								// dummy transfer
	call spitx;		
	r6 = 0 (Z);								// dummy transfer
	call spitx;		
	r6 = 0 (Z);								// dummy transfer
	call spitx;		
	r6 = 0 (Z);								// dummy transfer
	call spitx;		
	//---------
	// read out the sector
	
	p5 = 256 (Z);								// read all 256 bytes
	csync;
	
	LSETUP(spitxread1, spitxread2) lc0 = p5;	// setup loop
	csync;
	
	p5.h = 0xFF80;								// store page here
	p5.l = 0x6000;
	csync;
	
spitxread1:
	r6 = 0 (Z);										
	call spitx;									// spitx(0);
spitxread2:										
	b[p5++] = r6;								// store r6 to p5
	
	//----------
	p5.h = HI(FIO_FLAG_S);	
	p5.l = LO(FIO_FLAG_S);
	r7.l = 0x0004;				
	csync;
	w[p5] = r7;						// PF2 to H
	
	//----------
	p5 = [SP++];
	r5 = [SP++];
	r6 = [SP++];
	r7 = [SP++];
	RETS = [SP++];
	ASTAT = [SP++];
	
	rts;

readpage.end:
///////////////////////////////////////////////////
	.global spitx;
	.type spitx,STT_FUNC; 
	.align 4; 

spitx:
	[--SP] = ASTAT; 
	[--SP] = RETS;
	[--SP] = r7;
	[--SP] = p5;
	//----------
	p5.h = HI(SPI_STAT);	
	p5.l = LO(SPI_STAT);
	csync;
	
spitxloop1:										// wait while the TX buffer is full
	r7 = w[p5];									// read status
	cc = bittst(r7, 3);							// get only TXS bit (SPI_TDBR Data Buffer Status)
	if cc jump spitxloop1;						// if TXS==0, TX buffer is not full
	//-------
	
	p5.h = HI(SPI_TDBR);	
	p5.l = LO(SPI_TDBR);
	csync;
	w[p5] = r6;									// send the data
	//-------

	p5.h = HI(SPI_STAT);	
	p5.l = LO(SPI_STAT);
	csync;

spitxloop2:										// wait while the RX buffer is not full
	r7 = w[p5];									// read status
	cc = bittst(r7, 5);							// get only RXS bit (SPI_RXBR Data Buffer Status)
	if !cc jump spitxloop2;						// if RXS==1, RX buffer is not empty
	//-------

	p5.h = HI(SPI_RDBR);	
	p5.l = LO(SPI_RDBR);
	csync;
	r6.l = w[p5];								// read the received data
	//----------
	p5 = [SP++];
	r7 = [SP++];
	RETS = [SP++];
	ASTAT = [SP++];
	
	rts;

spitx.end:
///////////////////////////////////////////////////
	.global spisetup;
	.type spisetup,STT_FUNC; 
	.align 4; 

spisetup:
	[--SP] = ASTAT; 
	[--SP] = RETS;
	[--SP] = r7;
	[--SP] = p5;
	//----------
	p5.h = HI(SPI_CTL);	
	p5.l = LO(SPI_CTL);
	r7 = 0x0000 (Z);
	csync;
	w[p5] = r7;
	//-----------
	p5.h = HI(SPI_CTL);	
	p5.l = LO(SPI_CTL);
	r7 = 0x1c05 (Z);
	csync;
	w[p5] = r7;
	//-----------
	p5.h = HI(SPI_FLG);	
	p5.l = LO(SPI_FLG);
	r7 = 0xff00 (Z);
	csync;
	w[p5] = r7;
	//-----------
	p5.h = HI(SPI_STAT);	
	p5.l = LO(SPI_STAT);
	r7 = 0x0056 (Z);
	csync;
	w[p5] = r7;
	//-----------
	p5.h = HI(SPI_BAUD);	
	p5.l = LO(SPI_BAUD);
	r7 = 0x014c (Z);
	csync;
	w[p5] = r7;
	//-----------
	p5.h = HI(SPI_CTL);	
	p5.l = LO(SPI_CTL);
	r7 = 0x5c05 (Z);
	csync;
	w[p5] = r7;
	//----------
	p5 = [SP++];
	r7 = [SP++];
	RETS = [SP++];
	ASTAT = [SP++];
	
	rts;

spisetup.end:
///////////////////////////////////////////////////

