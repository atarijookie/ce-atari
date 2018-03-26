#ifndef SoftI2CMaster_h
#define SoftI2CMaster_h

#include "../datatypes.h"

#define _SOFTI2CMASTER_VERSION 13  // software version of this library

class SoftI2CMaster
{

private:
  // 'initialized' will be:
  //    255 on startup,
  //    0 if beginTransmission() was called and successful,
  //    any other value if there was an error during beginTransmission().
  BYTE initialized;

  // private methods

  void i2c_writebit( BYTE c );
  BYTE i2c_readbit(void);
  void i2c_init(void);
  void i2c_start(void);
  void i2c_repstart(void);
  void i2c_stop(void);
  BYTE i2c_write( BYTE c );
  BYTE i2c_read( BYTE ack );

public:
  // public methods
  SoftI2CMaster();

  void scan(void);

  BYTE beginTransmission(BYTE address);
  BYTE beginTransmission(int address);
  BYTE endTransmission(void);
  BYTE write(BYTE data);
  void write(BYTE *data, BYTE len);
  void begin(void) {return;};
  BYTE requestFrom(int address);
  BYTE requestFrom(BYTE address);
  BYTE requestFrom(int address, int quantity);
  BYTE requestFrom(BYTE address, BYTE quantity);
  BYTE read( BYTE ack );
  BYTE read();
  BYTE readLast();

};

#endif
