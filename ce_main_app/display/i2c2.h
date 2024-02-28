#ifndef I2C_BIT_BANG_H_
#define I2C_BIT_BANG_H_

#include <stdint.h>
#include <unistd.h>
#include <ctime>
#include <climits>

#include <string>

#define  I2CBITDELAY 5

class i2c2
    {
    private:
        bool i2c_started;

    public:
        i2c2(uint32_t gpioScl, uint32_t gpioSda);

    private:
        bool read_SCL(); // Set SCL as input and return current level of line, 0 or 1
        bool read_SDA(); // Set SDA as input and return current level of line, 0 or 1
        void clear_SCL(); // Actively drive SCL signal low
        void clear_SDA(); // Actively drive SDA signal low
        void arbitration_lost(char *where);

        void i2c_sleep();
        void i2c_delay();

        void i2c_start_cond();
        void i2c_stop_cond();
        void i2c_write_bit(bool bit);
        bool i2c_read_bit();
        bool i2c_write_byte(bool send_start, bool send_stop, uint8_t byte);
        uint8_t i2c_read_byte(bool nack, bool send_stop);

        uint32_t gpioScl;
        uint32_t gpioSda;
    public:
        // This executes the SMBus write byte protocol, returning negative errno else zero on success.
        int32_t i2c_smbus_write_byte_data(uint8_t i2c_address, uint8_t command, uint8_t value);

        // This executes the SMBus read byte protocol, returning negative errno else a data byte received from the device.
        int32_t i2c_smbus_read_byte_data(uint8_t i2c_address, uint8_t command);

        // This executes the SMBus block write protocol, returning negative errno else zero on success.
        int32_t i2c_smbus_write_i2c_block_data (uint8_t i2c_address, uint8_t command, uint8_t length, const uint8_t * values);

        // This executes the SMBus block read protocol, returning negative errno else the number
        // of data bytes in the slave's response.
        int32_t i2c_smbus_read_i2c_block_data (uint8_t i2c_address, uint8_t command, uint8_t length, uint8_t* values);
    };

#endif /* I2C_BIT_BANG_H_ */
