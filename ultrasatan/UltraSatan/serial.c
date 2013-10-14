/******************************************************************************/
/*                                                                            */
/*  SERIAL.C:  Low Level Serial Routines                                      */
/*                                                                            */
/******************************************************************************/
#include "mydefines.h"
//----------------------------------
void uart_init(void)                    /* Initialize Serial Interface */
{
	// set 57600/115200,8,N,1 with SCLK = 133 MHz
	*pUART_GCTL = 0x0000; 
	ssync();
	
	*pUART_LCR = 0x0083;
	ssync();
	
	*pUART_DLH = 0x0000;
//	*pUART_DLL = 0x0090;			// 57600
	*pUART_DLL = 0x0048;			// 115200
	ssync();
	
	*pUART_GCTL = 0x0001; 
	ssync();

	*pUART_LCR = 0x0003;
	ssync();
}
//----------------------------------
void uart_putc(BYTE ch) 
{
	WORD stat;
	
	while(1)
	{
		stat = *pUART_LSR;
		stat = stat & 0x0040;
		
		if(stat)
			break;			
	}
	
	*pUART_THR = (WORD) ch;
}	
//----------------------------------
void uart_putchar(BYTE ch) 
{
  if (ch=='\n')  
    uart_putc('\r');
  
  uart_putc(ch);
}
//----------------------------------
void uart_puts(char* s)
{
	while (*s) 
		uart_putc(*s++);
}
//----------------------------------
void uart_prints (char* s)
{
	while (*s)
		uart_putchar(*s++);
}
//----------------------------------
BYTE uart_kbhit(void) 												// returns true if character in receive buffer
{
	WORD stat;
	
	stat = *pUART_LSR;
	stat = stat & 0x0001;
		
	if (stat) 
		return 1;
	else
		return 0;
}
//----------------------------------
BYTE uart_getc ( void )  												// Read Character from Serial Port
{    
	WORD stat;

	while(TRUE)
	{	
		stat = *pUART_LSR;
		stat = stat & 0x0001;
		
		if(stat)
			break;
	}

	return (*pUART_RBR);
}
//----------------------------------
void uart_outhexB(BYTE a)
{
	BYTE lo,hi;
	
	lo = a & 0x0f;
	hi = (a >> 4) & 0x0f;
	
	if(hi < 0x0a)
		hi += '0';
	else
		hi += 'a' - 10;
		
	if(lo < 0x0a)
		lo += '0';
	else
		lo += 'a' - 10;

	uart_putc(hi);
	uart_putc(lo);
}
//----------------------------------
void uart_outhexD(DWORD a)
{
	BYTE i,j,k,l;
	
	i = (a >> 24) & 0xff;
	j = (a >> 16) & 0xff;
	k = (a >>  8) & 0xff;
	l = (a      ) & 0xff;
	
	uart_putc('0');
	uart_putc('x');
	uart_outhexB(i);
	uart_outhexB(j);
	uart_outhexB(k);
	uart_outhexB(l);
}
//----------------------------------
void uart_outDec1(BYTE a)
{
	if(a > 9)
		a = 15;
	
	uart_putc(a + 48);
}
//----------------------------------
void uart_prints2(BYTE number, char *s)
{
	uart_putc('\n');
	uart_outhexB(number);
	uart_putc(32);
	uart_prints(s);
}
//----------------------------------
void fputD(WORD inp, BYTE *buffer)
{
 WORD Ti, St, De, Je;
 WORD Re;
 
 Ti = inp / 1000;
 Re = inp % 1000;
 
 St = Re / 100;
 Re = Re % 100;
  
 De = Re / 10;
 Je = Re % 10;
 
 Ti += 48;
 St += 48;
 De += 48;
 Je += 48;
 
	if(Ti==48)
	{
		Ti = 32;
		
		if(St==48)
			St = 32;
	}
 
	if(buffer==NULL)
	{
		if(Ti != 32)
			uart_putc(Ti);
			
		if(St != 32)
			uart_putc(St);
			
 		uart_putc(De);
 		uart_putc(Je);
	}
 else
 {
  	buffer[0] = Ti; 
  	buffer[1] = St; 
  	buffer[2] = De; 
  	buffer[3] = Je; 
 }
}
//---------------------------------


