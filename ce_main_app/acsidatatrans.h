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


class AcsiDataTrans: public DataTrans
{
public:
    AcsiDataTrans();
    ~AcsiDataTrans();

    //----------------
    // function for checking if the specified ATN is raised and if so, then get command bytes
    virtual bool waitForATN(int whichSpiCs, BYTE *inBuf);

    // function for sending / receiving data from/to lower levels of communication (e.g. to SPI)
    virtual void txRx(int whichSpiCs, int count, BYTE *sendBuffer, BYTE *receiveBufer);

    // returns how many data there is still to be transfered
    virtual WORD getRemainingLength(void);
    //----------------
    // following functions are for convenient gathering of data in handling functions, but work only up to ACSI_BUFFER_SIZE which is currently 1 MB

    virtual void clear(bool clearAlsoDataDirection=true);

    virtual void setStatus(BYTE stat);

    virtual void addDataByte(BYTE val);
    virtual void addDataWord(WORD val);
    virtual void addDataDword(DWORD val);
    
    virtual void addZerosUntilSize(DWORD finalBufferCnt);

    virtual void addDataBfr(const void *data, DWORD cnt, bool padToMul16);

    virtual void addDataCString(const char *data, bool padToMul16);	// including null terminator

    virtual void padDataToMul16(void);

    virtual bool recvData(BYTE *data, DWORD cnt);
    virtual void sendDataAndStatus(bool fromRetryModule = false);       // by default it's not a retry
    virtual void sendDataToFd(int fd);

    virtual void dumpDataOnce(void);

    virtual DWORD getCount(void);

    //----------------
    // following functions are used for large (>1 MB) block transfers (Scsi::readSectors(), Scsi::writeSectors()) and also by the convenient functions above
    
    virtual bool sendData_start         (DWORD totalDataCount, BYTE scsiStatus, bool withStatus);
    virtual bool sendData_transferBlock (BYTE *pData, DWORD dataCount);

    virtual bool recvData_start         (DWORD totalDataCount);
    virtual bool recvData_transferBlock (BYTE *pData, DWORD dataCount);

    virtual void sendStatusToHans       (BYTE statusByte);

private:
    BYTE    *buffer;
    DWORD   count;
    BYTE    status;

    bool    statusWasSet;
    int     dataDirection;

    CConSpi *com;

    BYTE    *recvBuffer;

    BYTE    txBuffer[TX_RX_BUFF_SIZE];
    BYTE    rxBuffer[TX_RX_BUFF_SIZE];
	
	bool	dumpNextData;
};

#endif // ACSIDATATRANS_H
