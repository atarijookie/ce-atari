#ifndef ACSIDATATRANS_H
#define ACSIDATATRANS_H

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

#define ACSI_CMD_SIZE               14

#define ACSI_BUFFER_SIZE            (1024*1024)
#define COMMAND_SIZE                10

#define ACSI_MAX_TRANSFER_SIZE_BYTES    (254 * 512)

// bridge functions return value
#define E_TimeOut           0
#define E_OK                1
#define E_OK_A1             2
#define E_CARDCHANGE        3
#define E_RESET             4
#define E_FAIL_CMD_AGAIN    5

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

    virtual bool recvData(BYTE *data, DWORD cnt);
    virtual void sendDataAndStatus(bool fromRetryModule = false);       // by default it's not a retry

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

    DWORD timeoutTime;
    BYTE brStat;

    void hwDataDirection(bool readNotWrite);
    void hwDirForRead(void);    // data pins direction read (from RPi to ST)
    void hwDirForWrite(void);   // data pins direction write (from ST to RPi)

    bool timeout(void);         // returns true if timeout
    void dataWrite(BYTE val);   // sets value to data pins
    BYTE dataRead(void);        // reads value from data pins

    void getCmdLengthFromCmdBytesAcsi(void);
    void getCommandFromST(BYTE *receiveBufer);

    BYTE PIO_writeFirst(void);
    BYTE PIO_write(void);
    void PIO_read(BYTE val);
    void DMA_read(BYTE val);
    BYTE DMA_write(void);

};

#endif // ACSIDATATRANS_H
