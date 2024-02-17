#ifndef __CE_EXTENSION_H__
#define __CE_EXTENSION_H__

#define TAG_CE      0x4345
#define RET_CEDD    0x43454444

#define TAG_CX      0x4358
#define RET_CEXT    0x43455854

#define DATA_DIR_WRITE  0
#define DATA_DIR_READ   1

#define STATUS_NO_RESPONSE      0xFF

typedef struct 
{
    uint8_t readNotWrite;   // non-zero for read operation, zero for write operation
    uint8_t cmd;            // ID will be added in CE_IDD
    uint8_t extensionId;    // handle of the extension returned when opening extension
    uint8_t functionId;     // function id we want to call
    uint8_t sectorCount;    // how many data should be transfered
    uint8_t arg1;           // argument 1 on raw call
    uint8_t arg2;           // argument 2 on raw call
    uint8_t* buffer;        // where the data should be sent from or stored to
    uint8_t  statusByte;    // status byte from the call will be stored here
} __attribute__((packed)) CEXcall;

void ceExtensionCall(CEXcall *cc);

#endif // __CE_EXTENSION_H__
