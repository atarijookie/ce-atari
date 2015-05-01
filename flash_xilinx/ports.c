/*******************************************************/
/* file: ports.c                                       */
/* abstract:  This file contains the routines to       */
/*            output values on the JTAG ports, to read */
/*            the TDO bit, and to read a byte of data  */
/*            from the prom                            */
/* Revisions:                                          */
/* 12/01/2008:  Same code as before (original v5.01).  */
/*              Updated comments to clarify instructions.*/
/*              Add print in setPort for xapp058_example.exe.*/
/*******************************************************/
#include <unistd.h>
#include <stdio.h>

#include "ports.h"
#include "raspberry.h"

extern FILE *in;

/* setPort:  Implement to set the named JTAG signal (p) to the new value (v).*/
/* if in debugging mode, then just set the variables */
void setPort(short p,short val)
{
	int value;
	
	if(val == 0) {				// convert input value to macro / enum from bcm2835 library
		value = LOW;
	} else {
		value = HIGH;
	}
	
	switch(p) {
		case TMS:	bcm2835_gpio_write(PIN_TMS, value);	break;
		case TDI:	bcm2835_gpio_write(PIN_TDI, value); break;
		
        case TCK:	usleep(1);  
                    bcm2835_gpio_write(PIN_TCK, value);	
                    break;
    }
}


/* toggle tck LH.  No need to modify this code.  It is output via setPort. */
void pulseClock()
{
    setPort(TCK,0);  /* set the TCK port to low  */
    setPort(TCK,1);  /* set the TCK port to high */
}


/* readByte:  Implement to source the next byte from your XSVF file location */
/* read in a byte of data from the prom */
void readByte(unsigned char *data)
{
    /* read from file */
    *data   = (unsigned char) fgetc( in );
}

/* readTDOBit:  Implement to return the current value of the JTAG TDO signal.*/
/* read the TDO bit from port */
unsigned char readTDOBit()
{
	unsigned char val;
	
	val = bcm2835_gpio_lev(PIN_TDO);
	
    return val;
}

/* waitTime:  Implement as follows: */
/* REQUIRED:  This function must consume/wait at least the specified number  */
/*            of microsec, interpreting microsec as a number of microseconds.*/
void waitTime(long microsec)
{
    /* 
	This implementation is valid for only XC9500/XL/XV, CoolRunner/II CPLDs, 
    XC18V00 PROMs, or Platform Flash XCFxxS/XCFxxP PROMs. 
	*/
	
    setPort( TCK, 0 );
    usleep(microsec);
}
