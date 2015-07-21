#ifndef _INIT_H_
#define _INIT_H_

#include <iom16v.h>
#include <macros.h>

void port_init(void);
void timer0_init(void);
void spi_init(void);
void uart0_init(void);
void init_devices(void);

void portInit_floatBus(void);
void portInit_outputBus(void);

#endif


