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
#define ATN_FW_VERSION          0x01        // followed by string with FW version (length: 4 WORDs - cmd, v[0], v[1], 0)
#define ATN_SECTOR_WRITTEN      0x03        // sent: 3, side (highest bit) + track #, current sector #
#define ATN_SEND_TRACK          0x04        // send the whole track
#define ATN_SEND_WHOLE_IMAGE    0x05        // send the whole image

// commands sent from host to device
#define CMD_WRITE_PROTECT_OFF       0x10
#define CMD_WRITE_PROTECT_ON        0x20
#define CMD_DISK_CHANGE_OFF         0x30
#define CMD_DISK_CHANGE_ON          0x40
#define CMD_CURRENT_SECTOR          0x50                            // followed by sector #
#define CMD_GET_FW_VERSION          0x60
#define CMD_SET_DRIVE_ID_0          0x70
#define CMD_SET_DRIVE_ID_1          0x80
#define CMD_CURRENT_TRACK           0x90                            // followed by track #
#define CMD_DRIVE_ENABLED           0xa0
#define CMD_DRIVE_DISABLED          0xb0
#define CMD_MARK_READ               0xF000                          // this is not sent from host, but just a mark that this WORD has been read and you shouldn't continue to read further
#define CMD_MARK_READ_BYTE          0xF0                            // this is not sent from host, but just a mark that this BYTE has been read and you shouldn't continue to read further
#define CMD_TRACK_STREAM_END        0xF000                          // this is the mark in the track stream that we shouldn't go any further in the stream
#define CMD_TRACK_STREAM_END_BYTE   0xF0                            // this is the mark in the track stream that we shouldn't go any further in the stream


///////////////////////////

#define ATN_VARIABLE_LEN                0xffff

#define TX_HEADER_SIZE                  10
#define ATN_SENDFWVERSION_LEN_TX        (TX_HEADER_SIZE + 12)
#define ATN_SENDTRACK_REQ_LEN_TX        (TX_HEADER_SIZE + 2)

///////////////////////////

#define PIN_WGATE           3
#define PIN_DRIVE_SEL       4
#define PIN_MOT_EN          5
#define PIN_DIR             6
#define PIN_STEP            7
#define PIN_SIDE1           9
#define PIN_ANALOG_BTNS     10
#define PIN_INDEX           11
#define PIN_TRACK00         12
#define PIN_WPROTECT        13
#define PIN_DSKCHG          21
#define PIN_DENSITY         38
#define PIN_FLCC_OE         47

#define PIN_SCK             1
#define PIN_CS              2
#define PIN_MISO            8
#define PIN_MOSI            14
#define PIN_MFM_RXE         48

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

// for bits 0-31
#define BIT_SET(PIN)    REG_WRITE(GPIO_OUT_W1TS_REG, (1 << PIN))
#define BIT_CLR(PIN)    REG_WRITE(GPIO_OUT_W1TC_REG, (1 << PIN))

// for bits 32-48, but needs to subtract 32
#define BIT_SET1(PIN)    REG_WRITE(GPIO_OUT1_W1TS_REG, (1 << (PIN - 32)))
#define BIT_CLR1(PIN)    REG_WRITE(GPIO_OUT1_W1TC_REG, (1 << (PIN - 32)))

#define BIT_IS_H1(PIN)   ((REG_READ(GPIO_IN1_REG) & (1 << (PIN - 32))) == (1 << (PIN - 32)))
#define BIT_IS_L1(PIN)   ((REG_READ(GPIO_IN1_REG) & (1 << (PIN - 32))) == 0)
#define BIT_LEVEL1(PIN)  (BIT_IS_L1(PIN) ? LOW : HIGH)


#define WRITEBUFFER_SIZE    1300

typedef struct 
{
    uint8_t buffer[WRITEBUFFER_SIZE];  // buffer for the written data
    uint16_t count;                    // count of WORDs in buffer 
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
#define MAX_TRACKS                  90

#define IMAGE_NOT_LOADED    0
#define IMAGE_REQUESTED     1
#define IMAGE_LOADED        2

typedef struct {
    bool loaded;
    int track;
    int side;
    uint8_t* data;
} SingleTrack;

#define STREAM_TABLE_ITEMS  20
#define STREAM_TABLE_SIZE   STREAM_TABLE_ITEMS

#define STREAM_TABLE_OFFSET 10                  // 10 bytes - the stream table starts at this offset, because first 5 words are empty (ATN + sizes + other)
#define STREAM_START_OFFSET (STREAM_TABLE_OFFSET + STREAM_TABLE_SIZE)

#define TAG_WRITE_START 0x80    // start of sector data
#define TAG_WRITE_END   0xc0    // end of sector data

#endif /* DEFS_H_ */
