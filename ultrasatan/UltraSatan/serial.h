#ifndef serial_h_
#define serial_h_

#include "mydefines.h"

void uart_init(void);

void uart_putc(BYTE ch); 
void uart_putchar (BYTE ch); 		// replaces \n with \r\n 

void uart_puts   (char* s);		// uses putc
void uart_prints (char* s);		// uses putchar
void uart_prints2 (BYTE number, char *s);

int uart_kbhit( void );
int uart_getc ( void );

void uart_outhexB(BYTE a);
void uart_outhexD(DWORD a);

void uart_outDec1(BYTE a);

void fputD(WORD inp, BYTE *buffer);

#endif
