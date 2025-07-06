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

#define PIN_WGATE       3
#define PIN_DRIVE_SEL   4
#define PIN_MOT_EN      5
#define PIN_DIR         6
#define PIN_STEP        7
#define PIN_WDATA       8
#define PIN_SIDE1       9
#define PIN_DENSITY     10
#define PIN_INDEX       11
#define PIN_TRACK00     12
#define PIN_WPROTECT    13
#define PIN_RDATA       14
#define PIN_DSKCHG      21
#define PIN_FLCC_OE     47

#define PIN_SCL             15      // output
#define PIN_SDA             16      // input/output
#define PIN_KEYB_TX         17
#define PIN_KEYB_TX_ORIG    18
#define PIN_KEYB_RX         19
#define PIN_TXD2            20
#define PIN_TXD_DEBUG       43
#define PIN_RXD_DEBUG       44

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

typedef struct {
    uint8_t stWantsTheStream;          // based on floppy SELECT0/1 signal and MOTOR ON signal
    uint8_t weAreReceivingTrack;       // based on if STEP pulse happened, if SPI is receiving the track or not

    uint8_t updatePosition;
    
    uint8_t outputsAreEnabled;         // this says whether currently the output pins are streaming the MFM stream or not    
} TOutputFlags;


#define WRITEBUFFER_SIZE    1300

typedef struct 
{
    uint8_t buffer[WRITEBUFFER_SIZE];  // buffer for the written data
    uint16_t count;                    // count of WORDs in buffer 

    uint8_t readyToSend;               // until we store all the data, don't 

    void *next;                     // pointer to the next available TAtnBuffer
} TWriteBuffer;

typedef struct {
    uint8_t track;
    uint8_t side;
} TDrivePosition;   

typedef struct {
    uint8_t side;
    uint8_t track;
    uint8_t sector;
} SStreamed;

#define READTRACKDATA_SIZE_BYTES    13800
#define MAX_TRACKS                  80

#define STREAM_TABLE_OFFSET (10/2)              // 10 bytes / 5 words - the stream table starts at this offset, because first 5 words are empty (ATN + sizes + other)

#endif /* DEFS_H_ */
