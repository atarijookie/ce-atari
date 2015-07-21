//ICC-AVR application builder : 20/07/2015 14:24:00
// Target : M16
// Crystal: 1.0000Mhz

#include <iom16v.h>
#include <macros.h>
#include "global.h"

// call this to make AVR not interfere with ST 
void portInit_floatBus(void)
{
    DDRA  = 0x00;                       // PORT A - all inputs
    PORTA = 0x00;                       // PORT A - no pull ups (float)

    DDRB  = SCK | MOSI | SD_CS;         // PORT B - few outputs, rest inputs
    PORTB = SD_CS;                      // PORT B - SD_CS is high, rest is low and floating inputs

    DDRC  = WE | SIPO_D;                // PORT C - few outputs, rest inputs
    PORTC = WE | SIPO_D;

    DDRD  = SIPO_OE | SIPO_CLK | DBG_TXT;           // PORT D - these are outputs
    PORTD = SIPO_OE | SIPO_CLK | BTN | IS_SLAVE;    // PORT D - these outputs high, these inputs with pull ups
}

// call this before writing to SRAM (when ST is in RESET state)
void portInit_outputBus(void)
{
    DDRA  = 0xff;                       // PORT A - all output
    PORTA = 0x00;                       // PORT A - all 0s

    DDRB  = SCK | MOSI | SD_CS | 0x0f;  // PORT B - most are outputs
    PORTB = SD_CS;                      // PORT B - SD_CS is high, rest is low

    DDRC  = 0xff;                       // PORT C - all outputs
    PORTC = WE | OE |CS | SIPO_D;       // this need to be high now

    DDRD  = D5 | D4 | SIPO_OE | SIPO_CLK | DBG_TXT; // PORT D - these are outputs
    PORTD = SIPO_OE | SIPO_CLK | BTN | IS_SLAVE;    // PORT D - these outputs high, these inputs with pull ups
}

// call this before reading from SRAM (when ST is in RESET state)
void portInit_inputBus(void)
{
    DDRA  = 0xfe;                                   // PORT A - all outputs, except D0
    PORTA = 0x00;                                   // PORT A - all 0s

    DDRB  = SCK | MOSI | SD_CS | A1;                // PORT B - most are outputs, data pins as input
    PORTB = SD_CS;                                  // PORT B - SD_CS is high, rest is low

    DDRC  = A8 | A7 | WE | OE | CS | SIPO_D;        // PORT C - most outputs, data as inputs
    PORTC = WE | OE |CS | SIPO_D;                   // this need to be high now

    DDRD  = SIPO_OE | SIPO_CLK | DBG_TXT;           // PORT D - these are outputs
    PORTD = SIPO_OE | SIPO_CLK | BTN | IS_SLAVE;    // PORT D - these outputs high, these inputs with pull ups
}

//TIMER0 initialize - prescale:256
// WGM: Normal
// desired value: 100Hz
// actual value: 100.160Hz (0.2%)
void timer0_init(void)
{
 TCCR0 = 0x00; //stop
 TCNT0 = 0xD9; //set count
 OCR0  = 0x27;  //set compare
 TCCR0 = 0x04; //start timer
}

//SPI initialize
// clock rate: 500000hz
void spi_init(void)
{
 SPCR = 0x50; //setup SPI
 SPSR = 0x01; //setup SPI
}

//UART0 initialize
// desired baud rate: 19200
// actual: baud rate:20833 (7.8%)
// char size: 5 bits
// parity: Disabled
void uart0_init(void)
{
 UCSRB = 0x00; //disable while setting baud rate
 UCSRA = 0x00;
 UCSRC = BIT(URSEL) | 0x00;
 UBRRL = 0x02; //set baud rate lo
 UBRRH = 0x00; //set baud rate hi
 UCSRB = 0x08;
}

//call this routine to initialize all peripherals
void init_devices(void)
{
 //stop errant interrupts until set up
 CLI(); //disable all interrupts
 portInit_floatBus();
 timer0_init();
 spi_init();
 uart0_init();

 MCUCR = 0x00;
 GICR  = 0x00;
 TIMSK = 0x01; //timer interrupt sources
 SEI(); //re-enable interrupts
 //all peripherals are now initialized
}

