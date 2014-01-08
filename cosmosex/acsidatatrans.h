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

// commands sent from host to device
#define CMD_ACSI_CONFIG					0x10
#define CMD_DATA_WRITE					0x20
#define CMD_DATA_READ					0x30
#define CMD_SEND_STATUS					0x40
#define CMD_DATA_MARKER					0xda

// data direction after command processing
#define DATA_DIRECTION_UNKNOWN      0
#define DATA_DIRECTION_READ         1
#define DATA_DIRECTION_WRITE        2

class AcsiDataTrans
{
public:
    AcsiDataTrans();
    ~AcsiDataTrans();

    void clear(void);

    void setStatus(BYTE stat);

    void addDataByte(BYTE val);
    void addDataWord(WORD val);
    void addDataDword(DWORD val);

    void addDataBfr(BYTE *data, DWORD cnt, bool padToMul16);
	
    void padDataToMul16(void);

    bool recvData(BYTE *data, DWORD cnt);
    void sendDataAndStatus(void);

    void setCommunicationObject(CConSpi *comIn);

private:
    BYTE    *buffer;
    DWORD   count;
    BYTE    status;

    bool    statusWasSet;
    int     dataDirection;

    CConSpi *com;

    BYTE    *recvBuffer;

    BYTE    txBuffer[520];
    BYTE    rxBuffer[520];

    bool waitForATN(BYTE atnCode, DWORD timeoutMs);
	DWORD getTickCount(void);
    void sendStatusAfterWrite(void);
};

#endif // ACSIDATATRANS_H
