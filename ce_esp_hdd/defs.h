#ifndef DEFS_H_
#define DEFS_H_

#include <arduino.h>

#define LOG_MORE    1
#define ALWAYS_INLINE inline __attribute__((always_inline))

#define WIFI_CAPTIVE_AP_NAME    "CosmosEx AP"

// #define RW_TASKS

#define DELAY_NS   asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");

#ifndef TRUE
    #define TRUE 1
#endif

#ifndef FALSE
    #define FALSE 0
#endif

#ifndef MIN
    #define MIN(X,Y)    ((X < Y) ? X : Y)
#endif

#ifndef MAX
    #define MAX(X,Y)    ((X > Y) ? X : Y)
#endif

#define PREFERENCES_RW_MODE false
#define PREFERENCES_RO_MODE true

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
// #define STATE_SEND_FW_VER                       10

///////////////////////////

#define ATN_VARIABLE_LEN                0xffff

#define TX_HEADER_SIZE                  10
#define ATN_SENDFWVERSION_LEN_TX        (TX_HEADER_SIZE + 10)
#define ATN_SENDACSICOMMAND_LEN_TX      (TX_HEADER_SIZE + 14)

///////////////////////////

#define PIN_D0          1
#define PIN_D1          2
#define PIN_D2          3
#define PIN_D3          4
#define PIN_D4          5
#define PIN_D5          6
#define PIN_D6          7
#define PIN_D7          8

#define PIN_CMD1ST      9       // input
#define PIN_EOT         10      // input
#define PIN_OUT_OE      11      // output
#define PIN_DRQ_TRIG    12      // output
#define PIN_FF12D       13      // output
#define PIN_INT_TRIG    14      // output

#define PIN_SCL         15      // output
#define PIN_SDA         16      // input/output
#define PIN_TXD_IKBD    17
#define PIN_RXD_IKBD    18
#define PIN_USB_DM      19
#define PIN_USB_DP      20
#define PIN_TXD_DEBUG   43
#define PIN_RXD_DEBUG   44

#define PIN_BOOT_BTN    0       // input

/*
// using digitalRead / digitalWrite from Arduino env to manipulate gpio
#define BIT_IS_H(PIN)   (digitalRead(PIN) == HIGH)
#define BIT_IS_L(PIN)   (digitalRead(PIN) == LOW)
#define BIT_LEVEL(PIN)  digitalRead(PIN)

#define BIT_SET(PIN)    digitalWrite(PIN, HIGH)
#define BIT_CLR(PIN)    digitalWrite(PIN, LOW)
*/

// using esp32 regs to read / set / clear gpio bits
#define BIT_IS_H(PIN)   ((REG_READ(GPIO_IN_REG) & (1 << PIN)) == (1 << PIN))
#define BIT_IS_L(PIN)   ((REG_READ(GPIO_IN_REG) & (1 << PIN)) == 0)
#define BIT_LEVEL(PIN)  (BIT_IS_L(PIN) ? LOW : HIGH)

#define BIT_SET(PIN)    REG_WRITE(GPIO_OUT_W1TS_REG, (1 << PIN))
#define BIT_CLR(PIN)    REG_WRITE(GPIO_OUT_W1TC_REG, (1 << PIN))


#define HDD_ACSI    1

#endif /* DEFS_H_ */
