#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "../gpio.h"
#include "swi2c.h"

#define  i2cbitdelay 5

#define  I2C_ACK  1
#define  I2C_NAK  0

#ifdef ONPC_NOTHING
    #define i2c_scl_release() {}
    #define i2c_sda_release() {}
    #define i2c_scl_lo()      {}
    #define i2c_sda_lo()      {}
    #define i2c_scl_hi()      {}
    #define i2c_sda_hi()      {}
#else
    #define i2c_scl_release() bcm2835_gpio_fsel(PIN_SCL, BCM2835_GPIO_FSEL_INPT);
    #define i2c_sda_release() bcm2835_gpio_fsel(PIN_SDA, BCM2835_GPIO_FSEL_INPT);

    // sets SCL low and drives output
    #define i2c_scl_lo()    { bcm2835_gpio_write(PIN_SCL, LOW); bcm2835_gpio_fsel(PIN_SCL, BCM2835_GPIO_FSEL_OUTP); bcm2835_gpio_write(PIN_SCL, LOW); }

    // sets SDA low and drives output
    #define i2c_sda_lo()    { bcm2835_gpio_write(PIN_SDA, LOW); bcm2835_gpio_fsel(PIN_SDA, BCM2835_GPIO_FSEL_OUTP); bcm2835_gpio_write(PIN_SDA, LOW); }

    // set SCL high and to input (releases pin) (i.e. change to input, turn on pull up)
    #define i2c_scl_hi()    { bcm2835_gpio_write(PIN_SCL, HIGH); bcm2835_gpio_fsel(PIN_SCL, BCM2835_GPIO_FSEL_INPT); }

    // set SDA high and to input (releases pin) (i.e. change to input,turn on pull up)
    #define i2c_sda_hi()    { bcm2835_gpio_write(PIN_SDA, HIGH); bcm2835_gpio_fsel(PIN_SDA, BCM2835_GPIO_FSEL_INPT); }
#endif

//
// Constructor
//
SoftI2CMaster::SoftI2CMaster()
{
    i2c_init();
}

BYTE SoftI2CMaster::beginTransmission(BYTE address)
{
    i2c_start();
    BYTE rc = i2c_write((address<<1) | 0); // clr read bit
    // The standard Wire library returns a status in endTransmission(), not beginTransmission().
    // So we will return the status here but also remember the result so we can return it in endTransmission().
    // It also allows us to disable other I2C functions until beginTransmission has been called, if we want.
    initialized = rc;
    return rc;
}

//
BYTE SoftI2CMaster::requestFrom(BYTE address)
{
    i2c_start();
    BYTE rc = i2c_write((address<<1) | 1); // set read bit
    return rc;
}
//
BYTE SoftI2CMaster::requestFrom(int address)
{
    return requestFrom( (BYTE) address);
}
// Added for compatibility with the standard Wire library.
BYTE SoftI2CMaster::requestFrom(int address, int quantity)
{
    return requestFrom( (BYTE) address);

    // Ignore 'quantity', since SoftI2CMaster::requestFrom() just sets the start of read adresses,
    // so it's the same for any number of bytes.
    (void)quantity;
}
// Added for compatibility with the standard Wire library.
BYTE SoftI2CMaster::requestFrom(BYTE address, BYTE quantity)
{
    return requestFrom( (BYTE) address);

    // Ignore 'quantity', since SoftI2CMaster::requestFrom() just sets the start of read adresses,
    // so it's the same for any number of bytes.
    (void)quantity;
}

//
BYTE SoftI2CMaster::beginTransmission(int address)
{
    return beginTransmission((BYTE)address);
}

//
//
//
BYTE SoftI2CMaster::endTransmission(void)
{
    i2c_stop();
    return initialized;   // Use the result of beginTransmission()
}

// must be called in:
// slave tx event callback
// or after beginTransmission(address)
BYTE SoftI2CMaster::write(BYTE data)
{
    return i2c_write(data);
}

// must be called in:
// slave tx event callback
// or after beginTransmission(address)
void SoftI2CMaster::write(BYTE* data, BYTE quantity)
{
    for(BYTE i = 0; i < quantity; ++i){
        write(data[i]);
    }
}

// must be called in:
// slave tx event callback
// or after beginTransmission(address)
void SoftI2CMaster::write(char* data)
{
    write((BYTE*)data, strlen(data));
}

// must be called in:
// slave tx event callback
// or after beginTransmission(address)
void SoftI2CMaster::write(int data)
{
    write((BYTE)data);
}

//--------------------------------------------------------------------


void SoftI2CMaster::i2c_writebit( BYTE c )
{
    if ( c > 0 ) {
        i2c_sda_hi();
    } else {
        i2c_sda_lo();
    }

    i2c_scl_hi();
    usleep(i2cbitdelay);

    i2c_scl_lo();
    usleep(i2cbitdelay);

    if ( c > 0 ) {
        i2c_sda_lo();
    }
    usleep(i2cbitdelay);
}

//
BYTE SoftI2CMaster::i2c_readbit(void)
{
    i2c_sda_hi();
    i2c_scl_hi();
    usleep(i2cbitdelay);

    #ifdef ONPC_NOTHING
        BYTE c = HIGH;
    #else
        bcm2835_gpio_fsel(PIN_SDA, BCM2835_GPIO_FSEL_INPT); // SDA as input
        BYTE c = bcm2835_gpio_lev(PIN_SDA);                 // read SDA level
    #endif

    i2c_scl_lo();
    usleep(i2cbitdelay);

    return (c == HIGH);
}

// Inits bitbanging port, must be called before using the functions below
//
void SoftI2CMaster::i2c_init(void)
{
    i2c_sda_hi();
    i2c_scl_hi();

    usleep(i2cbitdelay);
}

// Send a START Condition
//
void SoftI2CMaster::i2c_start(void)
{
    // set both to high at the same time
    //I2C_DDR &=~ (_BV( I2C_SDA ) | _BV( I2C_SCL ));
    //*_sclDirReg &=~ (_sdaBitMask | _sclBitMask);
    i2c_sda_hi();
    i2c_scl_hi();

    usleep(i2cbitdelay);

    i2c_sda_lo();
    usleep(i2cbitdelay);

    i2c_scl_lo();
    usleep(i2cbitdelay);
}

void SoftI2CMaster::i2c_repstart(void)
{
    // set both to high at the same time (releases drive on both lines)
    //I2C_DDR &=~ (_BV( I2C_SDA ) | _BV( I2C_SCL ));
    //*_sclDirReg &=~ (_sdaBitMask | _sclBitMask);
    i2c_sda_hi();
    i2c_scl_hi();

    i2c_scl_lo();                           // force SCL low
    usleep(i2cbitdelay);

    i2c_sda_release();                      // release SDA
    usleep(i2cbitdelay);

    i2c_scl_release();                      // release SCL
    usleep(i2cbitdelay);

    i2c_sda_lo();                           // force SDA low
    usleep(i2cbitdelay);
}

// Send a STOP Condition
//
void SoftI2CMaster::i2c_stop(void)
{
    i2c_scl_hi();
    usleep(i2cbitdelay);

    i2c_sda_hi();
    usleep(i2cbitdelay);
}

// write a byte to the I2C slave device
//
BYTE SoftI2CMaster::i2c_write( BYTE c )
{
    for ( BYTE i=0;i<8;i++) {
        i2c_writebit( c & 128 );
        c<<=1;
    }

    return i2c_readbit();
}

// read a byte from the I2C slave device
//
BYTE SoftI2CMaster::i2c_read( BYTE ack )
{
    BYTE res = 0;

    for ( BYTE i=0;i<8;i++) {
        res <<= 1;
        res |= i2c_readbit();
    }

    if ( ack )
        i2c_writebit( 0 );
    else
        i2c_writebit( 1 );

    usleep(i2cbitdelay);

    return res;
}

// FIXME: this isn't right, surely
BYTE SoftI2CMaster::read( BYTE ack )
{
  return i2c_read( ack );
}

//
BYTE SoftI2CMaster::read()
{
    return i2c_read( I2C_ACK );
}

//
BYTE SoftI2CMaster::readLast()
{
    return i2c_read( I2C_NAK );
}

void SoftI2CMaster::scan(void)
{
    printf("I2C scanner\n");

    i2c_init();

    int addr;
    for(addr=0; addr<256; addr++) {
        if(addr % 16 == 0) {
            printf("\n%02x: ", addr);
        }

        int res = beginTransmission(addr);
        endTransmission();

        printf("%d ", res == 0);
    }
}
