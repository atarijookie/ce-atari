#ifndef DEFS_H_
#define DEFS_H_

#include <arduino.h>

#define LOG_MORE    1

#ifndef MIN
    #define MIN(X,Y)    ((X < Y) ? X : Y)
#endif

#ifndef MAX
    #define MAX(X,Y)    ((X > Y) ? X : Y)
#endif

#define CMD_TIMEOUT_SECS_PER_MB 3
#define CMD_TIMEOUT_ONESECOND   1000
#define CMD_TIMEOUT_SHORT       (CMD_TIMEOUT_ONESECOND / 2)     // this period will be 0.5 second
#define CMD_TIMEOUT_LONG        (CMD_TIMEOUT_ONESECOND * 3)     // this period will be 3.0 second

// commands sent from device to host
#define ATN_FW_VERSION                      0x01                                // followed by string with FW version (length: 4 uint16_ts - cmd, v[0], v[1], 0)
#define ATN_ACSI_COMMAND                    0x02
// #define ATN_READ_MORE_DATA                  0x03
#define ATN_WRITE_MORE_DATA                 0x04
// #define ATN_GET_STATUS                      0x05

// commands sent from host to device
#define CMD_ACSI_CONFIG                     0x10
#define CMD_DATA_WRITE                      0x20
#define CMD_DATA_READ_WITH_STATUS           0x30
#define CMD_SEND_STATUS                     0x40
#define CMD_DATA_READ_WITHOUT_STATUS        0x50
#define CMD_FLOPPY_CONFIG                   0x70
#define CMD_FLOPPY_SWITCH                   0x80
#define CMD_DATA_MARKER                     0xda

// these states define if the device should get command or transfer data
#define STATE_GET_COMMAND                       0
// #define STATE_SEND_COMMAND                   1
#define STATE_WAIT_COMMAND_RESPONSE             2
#define STATE_DATA_READ_WITH_STATUS             3
#define STATE_DATA_READ_WITHOUT_STATUS          4
#define STATE_DATA_WRITE                        5
#define STATE_WAIT_FOR_STATUS_ARRIVAL           6
#define STATE_READ_STATUS                       7

#define STATE_SEND_FW_VER                       10
#define STATE_SEND_WRITE_MORE_DATA              11
#define STATE_DATA_READ                         12

///////////////////////////

#define ATN_VARIABLE_LEN                0xffff

#define TX_HEADER_SIZE                  10
#define ATN_SENDFWVERSION_LEN_TX        (TX_HEADER_SIZE + 12)
#define ATN_SENDACSICOMMAND_LEN_TX      (TX_HEADER_SIZE + 14)

///////////////////////////

#define DATA_PINS_MASK      ((1 << PIN_D0) | (1 << PIN_D1) | (1 << PIN_D2) | (1 << PIN_D3) | (1 << PIN_D4) | (1 << PIN_D5) | (1 << PIN_D6) | (1 << PIN_D7))

#define PIN_D0              0
#define PIN_D1              1
#define PIN_D2              2
#define PIN_D3              3
#define PIN_D4              4
#define PIN_D5              5
#define PIN_D6              6
#define PIN_D7              7
#define PIN_SEL_IO_DP_SDA   8
#define PIN_RST_CD_REQ_SCL  9
#define PIN_ACK             10      // in
#define PIN_IN_OE           11      // out
#define PIN_OUT_OE          12      // out
#define PIN_OUT_LE1         13      // out
#define PIN_OUT_LE2         14      // out
#define PIN_ATN_MSG         26
#define PIN_BSY             27
#define PIN_DATA_DIR        28      // out

#define PIN_LED_EVB     25

#define BIT_IS_H(PIN)   (gpio_get(PIN) != 0)
#define BIT_IS_L(PIN)   (gpio_get(PIN) == 0)
#define BIT_LEVEL(PIN)  ( (gpio_get(PIN) == 0) ? 0 : 1)

#define BIT_SET(PIN)    gpio_put(PIN, true)
#define BIT_CLR(PIN)    gpio_put(PIN, false)

#endif /* DEFS_H_ */
