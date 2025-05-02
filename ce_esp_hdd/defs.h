#ifndef DEFS_H_
#define DEFS_H_

#include <arduino.h>

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

#define TX_HEADER_SIZE          10

#define PREFERENCES_RW_MODE false
#define PREFERENCES_RO_MODE true

#define CMD_TIMEOUT_SECS_PER_MB 3
#define CMD_TIMEOUT_ONESECOND   1000
#define CMD_TIMEOUT_SHORT       (CMD_TIMEOUT_ONESECOND / 2)     // this period will be 0.5 second
#define CMD_TIMEOUT_LONG        (CMD_TIMEOUT_ONESECOND * 3)     // this period will be 3.0 second

typedef struct 
{
    uint8_t buffer[550];           // buffer for the written data - with some (38 bytes) reserve at the end in case of overflow
    uint16_t count;                // count of uint16_ts in buffer 
    
    void *next;                    // pointer to the next available TAtnBuffer
} TWriteBuffer;

typedef struct 
{
    uint8_t buffer[550];            // buffer for the written data - with some (38 bytes) reserve at the end in case of overflow
    uint16_t count;                 // count of uint16_ts in buffer - may include header, markers, data, terminating marker
    uint16_t dataBytesCount;        // count of uint8_ts of just data in buffer
    
    void *next;                     // pointer to the next available TAtnBuffer
} TReadBuffer;

// commands sent from device to host
#define ATN_FW_VERSION                      0x01                                // followed by string with FW version (length: 4 uint16_ts - cmd, v[0], v[1], 0)
#define ATN_ACSI_COMMAND                    0x02
#define ATN_READ_MORE_DATA                  0x03
#define ATN_WRITE_MORE_DATA                 0x04
#define ATN_GET_STATUS                      0x05

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
#define STATE_SEND_COMMAND                      1
#define STATE_WAIT_COMMAND_RESPONSE             2
#define STATE_DATA_READ_WITH_STATUS             3
#define STATE_DATA_READ_WITHOUT_STATUS          4
#define STATE_DATA_WRITE                        5
#define STATE_WAIT_FOR_STATUS_ARRIVAL           6
#define STATE_READ_STATUS                       7
#define STATE_SEND_FW_VER                       10

#define CMD_BUFFER_LENGTH                       16

///////////////////////////
// The following definitions are definitions of how many uint16_ts are TXed and RXed for each different ATN.
// Note that ATN_VARIABLE_LEN shouldn't be used, and is replaced by some value.

#define ATN_VARIABLE_LEN                0xffff

#define ATN_SENDFWVERSION_LEN_TX        20
#define ATN_SENDACSICOMMAND_LEN_TX      24
#define ATN_READMOREDATA_LEN_TX         12
#define ATN_WRITEMOREDATA_LEN_TX        ATN_VARIABLE_LEN
#define ATN_GETSTATUS_LEN_TX            10

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
#define PIN_FF12D       12      // output
#define PIN_INT_TRIG    13      // output
#define PIN_DRQ_TRIG    14      // output

#define PIN_SCL         15      // output
#define PIN_SDA         16      // input/output
#define PIN_TXD_IKBD    17
#define PIN_RXD_IKBD    18
#define PIN_USB_DM      19
#define PIN_USB_DP      20
#define PIN_TXD_DEBUG   43
#define PIN_RXD_DEBUG   44
#define PIN_BOOT_BTN    0       // input

#define HDD_ACSI    1

#endif /* DEFS_H_ */
