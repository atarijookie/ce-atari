#ifndef DEFS_H_
#define DEFS_H_

#include <arduino.h>

#define LOG_MORE    1
#define LOG_LED     1
// #define LOG_FT200   1
// #define LOG_UART    1

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
#define HANDSHAKE_OUT_PINS  ((1 << PIN_DRQ) | (1 << PIN_INT))

#define PIN_TXD_DEBUG   0
#define PIN_RXD_DEBUG   1

#define PIN_D0          2
#define PIN_D1          3
#define PIN_D2          4
#define PIN_D3          5
#define PIN_D4          6
#define PIN_D5          7
#define PIN_D6          8
#define PIN_D7          9

#define PIN_DATA_DIR    10      // out
#define PIN_CS          11      // in
#define PIN_INT         12      // out
#define PIN_DRQ         13      // out
#define PIN_A1          22      // in
#define PIN_ACK         26      // in
#define PIN_RESET       27      // in

#define PIN_KEYB_RX     28

#define PIN_SDA         14      // input/output
#define PIN_SCL         15      // output

#define PIN_LED_EVB     25

#define BIT_IS_H(PIN)   (gpio_get(PIN) != 0)
#define BIT_IS_L(PIN)   (gpio_get(PIN) == 0)
#define BIT_LEVEL(PIN)  ( (gpio_get(PIN) == 0) ? 0 : 1)

#define BIT_SET(PIN)    gpio_put(PIN, true)
#define BIT_CLR(PIN)    gpio_put(PIN, false)

#ifdef LOG_LED
    #define LED_OFF         {}
    #define LED_ON          {}
    #define LED_TOGGLE      { debug(".\n"); }
#else
    #define LED_OFF         gpio_put(PIN_LED_EVB, 0)
    #define LED_ON          gpio_put(PIN_LED_EVB, 1)
    #define LED_TOGGLE      { gpio_put(PIN_LED_EVB, (gpio_get_out_level(PIN_LED_EVB) == 0) ? 1 : 0); }
#endif

#endif /* DEFS_H_ */
