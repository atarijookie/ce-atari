#ifndef ACSIDATATRANS_H
#define ACSIDATATRANS_H

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
#define CMD_ACSI_CONFIG					0x10
#define CMD_DATA_WRITE					0x20
#define CMD_DATA_READ					0x30
#define CMD_SEND_STATUS					0x40
#define CMD_FLOPPY_CONFIG               0x70
#define CMD_FLOPPY_SWITCH               0x80
#define CMD_DATA_MARKER					0xda

// data direction after command processing
#define DATA_DIRECTION_UNKNOWN      0
#define DATA_DIRECTION_READ         1
#define DATA_DIRECTION_WRITE        2

#define TX_RX_BUFF_SIZE             600

#define ACSI_CMD_SIZE               14

#define ACSI_BUFFER_SIZE            (1024*1024)
#define COMMAND_SIZE                10

#define ACSI_MAX_TRANSFER_SIZE_BYTES    (254 * 512)

#include "retrymodule.h"

class AcsiDataTrans
{
public:
    AcsiDataTrans();
    ~AcsiDataTrans();

    void setCommunicationObject(CConSpi *comIn);
    void setRetryObject(RetryModule *retryModule);

    void clear(bool clearAlsoDataDirection=true);

    void setStatus(BYTE stat);

    void addDataByte(BYTE val);
    void addDataWord(WORD val);
    void addDataDword(DWORD val);

    void addDataBfr(BYTE *data, DWORD cnt, bool padToMul16);
	
    void padDataToMul16(void);

    bool recvData(BYTE *data, DWORD cnt);
    void sendDataAndStatus(bool fromRetryModule = false);       // by default it's not a retry
    void sendDataToFd(int fd);
    
	void dumpDataOnce(void);

    DWORD getCount(void);
    
private:
    BYTE    *buffer;
    DWORD   count;
    BYTE    status;

    bool    statusWasSet;
    int     dataDirection;

    CConSpi     *com;
    RetryModule *retryMod;

    BYTE    *recvBuffer;

    BYTE    txBuffer[TX_RX_BUFF_SIZE];
    BYTE    rxBuffer[TX_RX_BUFF_SIZE];
	
	bool	dumpNextData;

    void sendStatusAfterWrite(void);
};

#endif // ACSIDATATRANS_H
