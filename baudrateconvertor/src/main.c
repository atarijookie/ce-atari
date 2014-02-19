#include "system_LPC8xx.h"
#include "lpc8xx.h"
#include "lpc8xx_uart.h"

#define BYTE				unsigned char
#define CIRCBUFFER_SIZE		64
#define CIRCBUFFER_POSMASK	0x3f

void setupUarts(void);
void SwitchMatrix_Init(void);

typedef struct {
	BYTE addPos;
	BYTE getPos;
	BYTE count;

	BYTE data[CIRCBUFFER_SIZE];
} TCircBuffer;

TCircBuffer buff0, buff1;

void circularInit(TCircBuffer *cb);
void cicrularAdd(TCircBuffer *cb, BYTE val);
BYTE cicrularGet(TCircBuffer *cb);

int main(void) {
	BYTE val;

	// setup clocks
	SystemInit();
	SystemCoreClockUpdate();

	// now map the UART pins to package pins
	SwitchMatrix_Init();

	// setup uarts. UART0 is 9600 baud, UART1 is 7812 baud
	setupUarts();

	// setup circular buffers
	circularInit(&buff0);
	circularInit(&buff1);

	// main retransmission loop
	// When received on UART0, send it to UART1.
	// When received on UART1, send it to UART0.
    while(1) {
    	if((LPC_USART0->STAT & RXRDY) != 0) {		// byte received?
    		val = LPC_USART0->RXDATA;				// read data from UART
    		cicrularAdd(&buff0, val);				// store it in circular buffer
    	}

    	if((LPC_USART1->STAT & RXRDY) != 0) {		// byte received?
    		val = LPC_USART1->RXDATA;				// read data from UART
    		cicrularAdd(&buff1, val);				// store it in circular buffer
    	}

    	if(buff0.count > 0) {						// something to send?
    		if((LPC_USART1->STAT & TXRDY) != 0) {	// byte can be sent?
    			val = cicrularGet(&buff0);			// get the byte
    			LPC_USART1->TXDATA = val;			// send it
    		}
    	}

    	if(buff1.count > 0) {						// something to send?
    		if((LPC_USART0->STAT & TXRDY) != 0) {	// byte can be sent?
    			val = cicrularGet(&buff1);			// get the byte
    			LPC_USART0->TXDATA = val;			// send it
    		}
    	}
    }

    return 0;
}

void circularInit(TCircBuffer *cb)
{
	BYTE i;

	// set vars to zero
	cb->addPos = 0;
	cb->getPos = 0;
	cb->count = 0;

	// fill data with zeros
	for(i=0; i<CIRCBUFFER_SIZE; i++) {
		cb->data[i] = 0;
	}
}

void cicrularAdd(TCircBuffer *cb, BYTE val)
{
	// if buffer full, fail
	if(cb->count >= CIRCBUFFER_SIZE) {
		return;
	}

	// store data at the right position
	cb->data[ cb->addPos ] = val;

	// increment and fix the add position
	cb->addPos++;
	cb->addPos = cb->addPos & CIRCBUFFER_POSMASK;
}

BYTE cicrularGet(TCircBuffer *cb)
{
	BYTE val;

	// if buffer empty, fail
	if(cb->count == 0) {
		return 0;
	}

	// buffer not empty, get data
	val = cb->data[ cb->getPos ];

	// increment and fix the get position
	cb->getPos++;
	cb->getPos = cb->getPos & CIRCBUFFER_POSMASK;

	// return value from buffer
	return val;
}

// this function is generated using 'Switch Matrix Tool' from NXP
void SwitchMatrix_Init(void)
{
    /* Enable SWM clock */
    LPC_SYSCON->SYSAHBCLKCTRL |= (1<<7);

    /* Pin Assign 8 bit Configuration */
    /* U0_TXD */
    /* U0_RXD */
    LPC_SWM->PINASSIGN0 = 0xffff0100UL;
    /* U1_RXD */
    LPC_SWM->PINASSIGN1 = 0xff04ffffUL;

    /* Pin Assign 1 bit Configuration */
    /* SWCLK */
    /* SWDIO */
    /* RESET */
    LPC_SWM->PINENABLE0 = 0xffffffb3UL;
}

void setupUarts(void)
{
	// disable UART interrupts
	NVIC_DisableIRQ(UART0_IRQn);
	NVIC_DisableIRQ(UART1_IRQn);

	// UART clk is divided by 1
	LPC_SYSCON->UARTCLKDIV = 1;

	// Enable UART0 and UART1 clock
	LPC_SYSCON->SYSAHBCLKCTRL |= (1<<18) | (1<<15) | (1<<14);

	// Peripheral reset control to UART, a "1" bring it out of reset
	LPC_SYSCON->PRESETCTRL &= ~((1<<3) | (1<<4));
	LPC_SYSCON->PRESETCTRL |=  ((1<<3) | (1<<4));

	uint32_t UARTSysClk = SystemCoreClock / 16;

	LPC_USART0->CFG = DATA_LENG_8|PARITY_NONE|STOP_BIT_1;	// 8 bits, no Parity, 1 Stop bit
	LPC_USART1->CFG = DATA_LENG_8|PARITY_NONE|STOP_BIT_1;	// 8 bits, no Parity, 1 Stop bit

	// set the baud rate
	LPC_USART0->BRG = (UARTSysClk / 9600 ) - 1;				// for 9600   baud
	LPC_USART1->BRG = (UARTSysClk / 7812 ) - 1;				// for 7812.5 baud

	// Clear all status bits
	LPC_USART0->STAT = CTS_DELTA | DELTA_RXBRK;
	LPC_USART1->STAT = CTS_DELTA | DELTA_RXBRK;

	// no UART ints
	LPC_USART0->INTENCLR = RXRDY | TXRDY | DELTA_RXBRK;
	LPC_USART1->INTENCLR = RXRDY | TXRDY | DELTA_RXBRK;

	// enable UARTs
	LPC_USART0->CFG |= UART_EN;
	LPC_USART1->CFG |= UART_EN;
}


