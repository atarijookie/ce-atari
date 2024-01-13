#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "gpio4.h"
#include "debug.h"

void spi_init(void);

/* Notes:

Pins states remain the same even after bcm2835_close() and even after prog termination.
bcm2835_gpio_write doesn't influence SPI CS pins, they are controlled by SPI part of the library.
*/

//------------------------------------------------------------------------------------------------------------------------

bool gpio4_open(void)
{
#ifndef ONPC
    if(geteuid() != 0) {
        Debug::out(LOG_ERROR, "The bcm2835 library requires to be run as root, try again...");
        return false;
    }

    // try to init the GPIO library
    if (!bcm2835_init()) {
        Debug::out(LOG_ERROR, "bcm2835_init failed, can't use GPIO.");
        return false;
    }

    // set these as inputs
    int inputs[11] = {DATA0, DATA1, DATA2, DATA3, DATA4, DATA5, DATA6, DATA7, CMD1ST, EOT, PIN_ATN_FRANZ};
    for(int i=0; i<11; i++) {
        bcm2835_gpio_fsel(inputs[i],  BCM2835_GPIO_FSEL_INPT);
    }

    // configure those as outputs
    int outputs[7] = {INT_TRIG, DRQ_TRIG, FF12D, IN_OE, OUT_OE, PIN_RESET_FRANZ, PIN_BOOT0_FRANZ};
    int outVals[7] = {LOW,      LOW,      LOW,   HIGH,  HIGH  , HIGH           , LOW};
    for(int i=0; i<7; i++) {
        bcm2835_gpio_fsel(outputs[i],  BCM2835_GPIO_FSEL_OUTP);
        bcm2835_gpio_write(outputs[i], outVals[i]);
    }

    // reset INT and DRQ so they won't block ACSI bus
    bcm2835_gpio_write(FF12D,    HIGH);     // we want the signals to go H
    bcm2835_gpio_write(INT_TRIG, HIGH);     // do CLK pulse
    bcm2835_gpio_write(DRQ_TRIG, HIGH);
    bcm2835_gpio_write(FF12D,    LOW);      // we can put this back to L
    bcm2835_gpio_write(INT_TRIG, LOW);      // CLK back to L
    bcm2835_gpio_write(DRQ_TRIG, LOW);

    spi_init();
#endif

    return true;
}

void gpio4_close(void)
{
#ifndef ONPC
    // before terminating set the RESET pin of STM32 as input so we can work with it through SWD
    // (otherwise ST-LINK will fail to reset it)
    bcm2835_gpio_fsel(PIN_RESET_FRANZ,      BCM2835_GPIO_FSEL_INPT);
    bcm2835_spi_end();          // end the SPI stuff here
    bcm2835_close();            // close the GPIO library and finish
#endif
}
