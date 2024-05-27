#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef ONPC
    #include <bcm2835.h>
#endif

#include "gpio_rascsi.h"
#include "debug.h"

//------------------------------------------------------------------------------------------------------------------------

bool gpiorascsi_open(void)
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
    int inputs[13] = {
        DATA0, DATA1, DATA2, DATA3, DATA4, DATA5, DATA6, DATA7, DATAP, 
        PIN_SEL, PIN_RST, PIN_ACK, PIN_ATN};
    for(int i=0; i<13; i++) {
        bcm2835_gpio_fsel(inputs[i],  BCM2835_GPIO_FSEL_INPT);
    }

    // configure those as outputs
    int outputs[12] = {PIN_ACT, PIN_ENB, PIN_IO, PIN_REQ, PIN_CD, PIN_MSG, PIN_TAD, PIN_IND, PIN_DTD, PIN_BSY, PIN_SDA, PIN_SCL};
    int outVals[12] = {LOW,     LOW,     HIGH,   HIGH,    HIGH,   HIGH,    LOW,     LOW,     HIGH,    HIGH,    HIGH,    HIGH};
    for(int i=0; i<12; i++) {
        bcm2835_gpio_fsel(outputs[i],  BCM2835_GPIO_FSEL_OUTP);
        bcm2835_gpio_write(outputs[i], outVals[i]);
    }
#endif

    return true;
}

void gpiorascsi_close(void)
{
#ifndef ONPC
    bcm2835_close();            // close the GPIO library and finish
#endif
}
