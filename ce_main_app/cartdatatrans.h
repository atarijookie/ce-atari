#ifndef CARTDATATRANS_H
#define CARTDATATRANS_H

#include "datatrans.h"
#include "conspi.h"
#include "datatypes.h"

// commands sent from device to host
#define ATN_FW_VERSION					0x01								// followed by string with FW version (length: 4 WORDs - cmd, v[0], v[1], 0)
#define ATN_ACSI_COMMAND				0x02
#define ATN_READ_MORE_DATA				0x03
#define ATN_WRITE_MORE_DATA				0x04
#define ATN_GET_STATUS					0x05
#define ATN_ANY							0xff								// this is used only on host to wait for any ATN


// commands sent from host to device
#define CMD_ACSI_CONFIG                 0x10
#define CMD_DATA_WRITE                  0x20
#define CMD_DATA_READ_WITH_STATUS       0x30
#define CMD_SEND_STATUS                 0x40
#define CMD_DATA_READ_WITHOUT_STATUS    0x50
#define CMD_FLOPPY_CONFIG               0x70
#define CMD_FLOPPY_SWITCH               0x80
#define CMD_DATA_MARKER                 0xda

// data direction after command processing
#define DATA_DIRECTION_UNKNOWN      0
#define DATA_DIRECTION_READ         1
#define DATA_DIRECTION_WRITE        2

#define TX_RX_BUFF_SIZE             600

#define COMMAND_SIZE                10

#define ACSI_MAX_TRANSFER_SIZE_BYTES    (254 * 512)

// bridge functions return value
#define E_TimeOut           0
#define E_OK                1
#define E_OK_A1             2
#define E_CARDCHANGE        3
#define E_RESET             4
#define E_FAIL_CMD_AGAIN    5

// definition of configuration bytes for FPGA
#define CNF_PIO_WRITE_FIRST 0   // PIO write 1st - waiting for 1st cmd byte, XATN shows FIFO-not-empty (can read)
#define CNF_PIO_WRITE       1   // PIO write - data from ST to RPi, XATN shows FIFO-not-empty (can read)
#define CNF_DMA_WRITE       2   // DMA write - data from ST to RPi, XATN shows FIFO-not-empty (can read)
#define CNF_MSG_WRITE       3   // MSG write - data from ST to RPi, XATN shows FIFO-not-empty (can read)
#define CNF_PIO_READ        4   // PIO read  - data from RPi to ST, XATN shows FIFO-not-full  (can write)
#define CNF_DMA_READ        5   // DMA read  - data from RPi to ST, XATN shows FIFO-not-full  (can write)
#define CNF_MSG_READ        6   // MSG read  - data from RPi to ST, XATN shows FIFO-not-full  (can write)
#define CNF_GET_FIFO_CNT    7   // read fifo_byte_count -- how many bytes are stored in FIFO and can be read out
#define CNF_SRAM_WRITE      13  // SRAM_write - store another byte into boot SRAM (index register is reset to zero with reset)
#define CNF_CURRENT_IF      14  // current IF - to determine, if it's ACSI, SCSI or CART (because SCSI needs extra MSG phases)
#define CNF_CURRENT_VERSION 15  // current version - to determine, if this chip needs update or not

// definition of FPGA pins
#ifndef ONPC_NOTHING
    #define PIN_XATN            PIN_ATN_HANS
    #define PIN_XDnC            RPI_V2_GPIO_P1_32
    #define PIN_XRnW            RPI_V2_GPIO_P1_26
    #define PIN_XRESET          PIN_RESET_HANS
    #define PIN_XNEXT           RPI_V2_GPIO_P1_24

    #define PIN_D0              RPI_V2_GPIO_P1_12
    #define PIN_D1              RPI_V2_GPIO_P1_35
    #define PIN_D2              RPI_V2_GPIO_P1_38
    #define PIN_D3              RPI_V2_GPIO_P1_40
    #define PIN_D4              RPI_V2_GPIO_P1_15
    #define PIN_D5              RPI_V2_GPIO_P1_18
    #define PIN_D6              RPI_V2_GPIO_P1_22
    #define PIN_D7              RPI_V2_GPIO_P1_37
#else
    #define PIN_XATN            0
    #define PIN_XDnC            0
    #define PIN_XRnW            0
    #define PIN_XRESET          0
    #define PIN_XNEXT           0

    #define PIN_D0              0
    #define PIN_D1              0
    #define PIN_D2              0
    #define PIN_D3              0
    #define PIN_D4              0
    #define PIN_D5              0
    #define PIN_D6              0
    #define PIN_D7              0
#endif

class CartDataTrans: public DataTrans
{
public:
    CartDataTrans();
    ~CartDataTrans();

    virtual void configureHw(void);
    //----------------
    // function for checking if the specified ATN is raised and if so, then get command bytes
    virtual bool waitForATN(int whichSpiCs, BYTE *inBuf);

    // function for sending / receiving data from/to lower levels of communication (e.g. to SPI)
    virtual void txRx(int whichSpiCs, int count, BYTE *sendBuffer, BYTE *receiveBufer);

    // returns how many data there is still to be transfered
    virtual WORD getRemainingLength(void);
    //----------------
    // following functions are used for large (>1 MB) block transfers (Scsi::readSectors(), Scsi::writeSectors()) and also by the convenient functions above
    
    virtual bool sendData_start         (DWORD totalDataCount, BYTE scsiStatus, bool withStatus);
    virtual bool sendData_transferBlock (BYTE *pData, DWORD dataCount);

    virtual bool recvData_start         (DWORD totalDataCount);
    virtual bool recvData_transferBlock (BYTE *pData, DWORD dataCount);

    virtual void sendStatusToHans       (BYTE statusByte);

private:
    BYTE cmd[32];
    int  cmdLen;
    bool gotCmd;

    DWORD timeoutTime;
    BYTE brStat;

    DWORD nextFakeFwAtn;

    BYTE fwYear, fwMonth, fwDay;    // fake Hans version - current date, to never update (not present) Hans

    void hwDataDirection(bool readNotWrite);
    void hwDirForRead(void);    // data pins direction read (from RPi to ST)
    void hwDirForWrite(void);   // data pins direction write (from ST to RPi)

    bool timeout(void);                 // returns true if timeout
    void setFPGAconfigByte(BYTE cnf);   // sets the config byte of FPGA to tell it what it should do
    void writeDataToGPIO(BYTE val);     // sets value to data pins
    BYTE readDataFromGPIO(void);        // reads value from data pins
    void resetCpld(void);

    int getCmdLengthFromCmdBytesAcsi(void);
    void getCommandFromST(void);

    BYTE PIO_writeFirst(void);
    BYTE PIO_write(void);
    void PIO_read(BYTE val);
    void DMA_read(BYTE val);
    BYTE DMA_write(void);

};

#endif // CARTDATATRANS_H
