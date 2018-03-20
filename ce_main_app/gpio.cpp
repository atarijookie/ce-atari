#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "gpio.h"
#include "debug.h"

void spi_init(void);

/* Notes:

Pins states remain the same even after bcm2835_close() and even after prog termination.
bcm2835_gpio_write doesn't influence SPI CS pins, they are controlled by SPI part of the library.
*/

//------------------------------------------------------------------------------------------------------------------------

#ifdef ONPC_GPIO
// the following dummy functions are here for compilation on PC (not RPi)

#include <signal.h>
#include <pthread.h>

#include "socks.h"

bool bcm2835_init(void) { return true; }
void bcm2835_gpio_fsel(int a, int b) {}
void bcm2835_spi_begin() {}
void bcm2835_spi_setBitOrder(int a) {}
void bcm2835_spi_setDataMode(int a) {}
void bcm2835_spi_setClockDivider(int a) {}
void bcm2835_spi_setChipSelectPolarity(int a, int b) {}
void bcm2835_spi_chipSelect(int a) {}
void bcm2835_spi_end(void) {}
void bcm2835_close(void) {}

int  bcm2835_gpio_lev(int a) {return 0; }

void bcm2835_spi_transfernb(char *txBuf, char *rxBuf, int c) { }

#define SYNC1           0xab
#define SYNC2           0xcd

#define FUN_GPIO_WRITE  1
#define FUN_SPI_TX_RX   2
#define FUN_SPI_ATN     3

pthread_mutex_t tcpMutex = PTHREAD_MUTEX_INITIALIZER;

void bcm2835_gpio_write(int a, int b)
{
    pthread_mutex_lock(&tcpMutex);

    BYTE bfrWrite[6];
    memset(bfrWrite, 0, 6);

    bfrWrite[0] = SYNC1;
    bfrWrite[1] = SYNC2;
    bfrWrite[2] = FUN_GPIO_WRITE;
    bfrWrite[3] = a;
    bfrWrite[4] = b;

    clientSocket_write(bfrWrite, 6);
    pthread_mutex_unlock(&tcpMutex);
}

void spi_tx_rx(int whichSpiCS, int count, BYTE *txBuf, BYTE *rxBuf)
{
    pthread_mutex_lock(&tcpMutex);

    BYTE bfrWrite[6];
    memset(bfrWrite, 0, 6);

    bfrWrite[0] = SYNC1;
    bfrWrite[1] = SYNC2;
    bfrWrite[2] = FUN_SPI_TX_RX;
    bfrWrite[3] = whichSpiCS;
    bfrWrite[4] = (BYTE) (count >> 8);
    bfrWrite[5] = (BYTE) (count & 0xff);

    clientSocket_write(bfrWrite, 6);
    clientSocket_write(txBuf, count);

    BYTE bfrRead[2];
    int res = clientSocket_read(bfrRead, 2);

    if(bfrRead[0] != SYNC1 || bfrRead[1] != SYNC2 || res != 2) {
        pthread_mutex_unlock(&tcpMutex);
        return;
    }

    clientSocket_read(rxBuf, count);
    pthread_mutex_unlock(&tcpMutex);
}

bool spi_atn(int whichSpiAtn)
{
    pthread_mutex_lock(&tcpMutex);

    BYTE bfrWrite[6];
    memset(bfrWrite, 0, 6);

    bfrWrite[0] = SYNC1;
    bfrWrite[1] = SYNC2;
    bfrWrite[2] = FUN_SPI_ATN;
    bfrWrite[3] = whichSpiAtn;

    clientSocket_write(bfrWrite, 6);

    BYTE bfrRead[3];
    int res = clientSocket_read(bfrRead, 3);

    pthread_mutex_unlock(&tcpMutex);

    if(bfrRead[0] != SYNC1 || bfrRead[1] != SYNC2 || res != 3) {
        return false;
    }

    bool b = bfrRead[2];

    return b;   // returns true if pin is high, returns false if pin is low
}
#endif

//------------------------------------------------------------------------------------------------------------------------

#if defined(ONPC_HIGHLEVEL) || defined(ONPC_NOTHING)
// the following dummy functions are here for compilation on PC - when HIGHLEVEL of emulation is done

bool bcm2835_init(void) { return true; }
void bcm2835_gpio_fsel(int a, int b) {}
void bcm2835_spi_begin() {}
void bcm2835_spi_setBitOrder(int a) {}
void bcm2835_spi_setDataMode(int a) {}
void bcm2835_spi_setClockDivider(int a) {}
void bcm2835_spi_setChipSelectPolarity(int a, int b) {}
void bcm2835_spi_chipSelect(int a) {}
void bcm2835_spi_end(void) {}
void bcm2835_close(void) {}
void bcm2835_gpio_write(int a, int b) {}
int  bcm2835_gpio_lev(int a) {return 0; }
void bcm2835_spi_transfernb(char *txBuf, char *rxBuf, int c) { }
void spi_tx_rx(int whichSpiCS, int count, BYTE *txBuf, BYTE *rxBuf){}
bool spi_atn(int whichSpiAtn) { return false; }
#endif

//------------------------------------------------------------------------------------------------------------------------

#if !defined(ONPC_GPIO) && !defined(ONPC_HIGHLEVEL) && !defined(ONPC_NOTHING)
void spi_tx_rx(int whichSpiCS, int count, BYTE *txBuf, BYTE *rxBuf)
{
    bcm2835_spi_chipSelect(whichSpiCS);
    bcm2835_spi_transfernb((char *) txBuf, (char *) rxBuf, count);
}

bool spi_atn(int whichSpiAtn)
{
    BYTE val;

    val = bcm2835_gpio_lev(whichSpiAtn);

    return (val == HIGH);                   // returns true if pin is high, returns false if pin is low
}
#endif

//------------------------------------------------------------------------------------------------------------------------

#ifdef ONPC_NOTHING

bool gpio_open(void) { return true; }
void spi_init(void)  {              }
void gpio_close(void){              }

#endif

#ifndef ONPC_NOTHING

bool gpio_open(void)
{
    #ifdef ONPC_GPIO
    clientSocket_setParams((char *) "192.168.123.142", 1111);
    clientSocket_createConnection();
    return true;
    #endif

    if(geteuid() != 0) {
        Debug::out(LOG_ERROR, "The bcm2835 library requires to be run as root, try again...");
        return false;
    }

    // try to init the GPIO library
    if (!bcm2835_init()) {
        Debug::out(LOG_ERROR, "bcm2835_init failed, can't use GPIO.");
        return false;
    }

    // pins for XILINX programming
    // outputs: TDI (GPIO3), TCK (GPIO4), TMS (GPIO17)
    bcm2835_gpio_fsel(PIN_TDI,  BCM2835_GPIO_FSEL_OUTP);        // TDI
    bcm2835_gpio_fsel(PIN_TMS,  BCM2835_GPIO_FSEL_OUTP);        // TMS
    bcm2835_gpio_fsel(PIN_TCK,  BCM2835_GPIO_FSEL_OUTP);        // TCK
    // inputs : TDO (GPIO2)
    bcm2835_gpio_fsel(PIN_TDO,  BCM2835_GPIO_FSEL_INPT);        // TDO

    bcm2835_gpio_write(PIN_TDI, LOW);
    bcm2835_gpio_write(PIN_TMS, LOW);
    bcm2835_gpio_write(PIN_TCK, LOW);


    // pins for both STM32 programming
    bcm2835_gpio_fsel(PIN_RESET_HANS,       BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(PIN_RESET_FRANZ,      BCM2835_GPIO_FSEL_OUTP);
//  bcm2835_gpio_fsel(PIN_TXD,              BCM2835_GPIO_FSEL_OUTP);
//  bcm2835_gpio_fsel(PIN_RXD,              BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_fsel(PIN_TX_SEL1N2,        BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(PIN_BOOT0_FRANZ_HANS, BCM2835_GPIO_FSEL_OUTP);

    bcm2835_gpio_write(PIN_TX_SEL1N2,           HIGH);
    bcm2835_gpio_write(PIN_BOOT0_FRANZ_HANS,    LOW);       // BOOT0: L means boot from flash, H means boot the boot loader

    bcm2835_gpio_write(PIN_RESET_HANS,          HIGH);      // reset lines to RUN (not reset) state
    bcm2835_gpio_write(PIN_RESET_FRANZ,         HIGH);

    bcm2835_gpio_fsel(PIN_BEEPER,           BCM2835_GPIO_FSEL_OUTP);

    // pins for communication with Franz and Hans
    bcm2835_gpio_fsel(PIN_ATN_HANS,         BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_fsel(PIN_ATN_FRANZ,        BCM2835_GPIO_FSEL_INPT);
//  bcm2835_gpio_fsel(PIN_CS_HANS,          BCM2835_GPIO_FSEL_OUTP);
//  bcm2835_gpio_fsel(PIN_CS_FRANZ,         BCM2835_GPIO_FSEL_OUTP);
//  bcm2835_gpio_fsel(PIN_MOSI,             BCM2835_GPIO_FSEL_OUTP);
//  bcm2835_gpio_fsel(PIN_MISO,             BCM2835_GPIO_FSEL_INPT);
//  bcm2835_gpio_fsel(PIN_SCK,              BCM2835_GPIO_FSEL_OUTP);

    spi_init();

    return true;
}

void spi_init(void)
{
    bcm2835_spi_begin();

    bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);        // The default
    bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);                     // The default

    bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_16);      // 16 =  64ns = 15.625MHz
    // according to SCK: 0,5625 us / byte = ~ 1.77 MB/s, but according to CS it's more like 1.6 MB/s

    bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);        // the default
    bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS1, LOW);        // the default

    bcm2835_spi_chipSelect(BCM2835_SPI_CS0);
}

void gpio_close(void)
{
    // before terminating set the RESET pins of both STM32 as inputs so we can work with them through SWD
    // (otherwise ST-LINK will fail to reset them)
    bcm2835_gpio_fsel(PIN_RESET_HANS,       BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_fsel(PIN_RESET_FRANZ,      BCM2835_GPIO_FSEL_INPT);

    bcm2835_spi_end();          // end the SPI stuff here

    bcm2835_close();            // close the GPIO library and finish
}

#endif


