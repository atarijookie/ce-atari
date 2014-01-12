#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "gpio.h"

void outDebugString(const char *format, ...);

void spi_init(void);

/* Notes:

Pins states remain the same even after bcm2835_close() and even after prog termination.
bcm2835_gpio_write doesn't influence SPI CS pins, they are controlled by SPI part of the library.
*/

bool gpio_open(void)
{
	if(geteuid() != 0) {
		outDebugString( "The bcm2835 library requires to be run as root, try again...");
        return false;
	}

	// try to init the GPIO library
	if (!bcm2835_init()) {
		outDebugString( "bcm2835_init failed, can't use GPIO.");
        return false;
	}
	
	// pins for XILINX programming	
	// outputs: TDI (GPIO3), TCK (GPIO4), TMS (GPIO17)
	bcm2835_gpio_fsel(PIN_TDI,	BCM2835_GPIO_FSEL_OUTP);		// TDI
	bcm2835_gpio_fsel(PIN_TMS,	BCM2835_GPIO_FSEL_OUTP);		// TMS
	bcm2835_gpio_fsel(PIN_TCK,	BCM2835_GPIO_FSEL_OUTP);		// TCK
	// inputs : TDO (GPIO2)
	bcm2835_gpio_fsel(PIN_TDO,	BCM2835_GPIO_FSEL_INPT);		// TDO

	bcm2835_gpio_write(PIN_TDI,	LOW);
	bcm2835_gpio_write(PIN_TMS,	LOW);
	bcm2835_gpio_write(PIN_TCK,	LOW);	
	
	
	// pins for both STM32 programming 
	bcm2835_gpio_fsel(PIN_RESET_HANS,		BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(PIN_RESET_FRANZ,		BCM2835_GPIO_FSEL_OUTP);
//	bcm2835_gpio_fsel(PIN_TXD,				BCM2835_GPIO_FSEL_OUTP);
//	bcm2835_gpio_fsel(PIN_RXD,				BCM2835_GPIO_FSEL_INPT);					
	bcm2835_gpio_fsel(PIN_TX_SEL1N2,		BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(PIN_BOOT0_FRANZ_HANS,	BCM2835_GPIO_FSEL_OUTP);

	bcm2835_gpio_write(PIN_TX_SEL1N2,			HIGH);
	bcm2835_gpio_write(PIN_BOOT0_FRANZ_HANS,	LOW);		// BOOT0: L means boot from flash, H means boot the boot loader
	
	bcm2835_gpio_write(PIN_RESET_HANS,			HIGH);		// reset lines to RUN (not reset) state
	bcm2835_gpio_write(PIN_RESET_FRANZ,			HIGH);

	
	// pins for communication with Franz and Hans
	bcm2835_gpio_fsel(PIN_ATN_HANS,			BCM2835_GPIO_FSEL_INPT);			
	bcm2835_gpio_fsel(PIN_ATN_FRANZ,		BCM2835_GPIO_FSEL_INPT);
//	bcm2835_gpio_fsel(PIN_CS_HANS,			BCM2835_GPIO_FSEL_OUTP);
//	bcm2835_gpio_fsel(PIN_CS_FRANZ,			BCM2835_GPIO_FSEL_OUTP);
//	bcm2835_gpio_fsel(PIN_MOSI,				BCM2835_GPIO_FSEL_OUTP);
//	bcm2835_gpio_fsel(PIN_MISO,				BCM2835_GPIO_FSEL_INPT);
//	bcm2835_gpio_fsel(PIN_SCK,				BCM2835_GPIO_FSEL_OUTP);
	
	spi_init();
	
	return true;
}

void spi_init(void)
{
    bcm2835_spi_begin();
	
    bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);		// The default
    bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);						// The default

    bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_16);		// 16 =  64ns = 15.625MHz
	// according to SCK: 0,5625 us / byte = ~ 1.77 MB/s, but according to CS it's more like 1.6 MB/s
    
	bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);		// the default
    bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS1, LOW);		// the default

    bcm2835_spi_chipSelect(BCM2835_SPI_CS0);						
}

void spi_tx_rx(int whichSpiCS, int count, BYTE *txBuf, BYTE *rxBuf)
{
    bcm2835_spi_chipSelect(whichSpiCS);
	bcm2835_spi_transfernb((char *) txBuf, (char *) rxBuf, count); 
}

bool spi_atn(int whichSpiAtn)
{
	BYTE val;
	
	val = bcm2835_gpio_lev(PIN_TDO); 
	
	return (val == HIGH);					// returns true if pin is high, returns false if pin is low
}

void gpio_close(void)
{
    bcm2835_spi_end();			// end the SPI stuff here
	
	bcm2835_close();			// close the GPIO library and finish
}



