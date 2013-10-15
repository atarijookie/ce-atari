#ifndef ACSIDATATRANS_H
#define ACSIDATATRANS_H

#include "cconusb.h"
#include "datatypes.h"

// commands sent from device to host

#define ATN_FW_VERSION					0x01								// followed by string with FW version (length: 4 WORDs - cmd, v[0], v[1], 0)
#define ATN_ACSI_COMMAND				0x02
#define ATN_READ_MORE_DATA				0x03
#define ATN_WRITE_MORE_DATA				0x04

// commands sent from host to device
#define CMD_ACSI_CONFIG					0x10
#define CMD_DATA_WRITE					0x20
#define CMD_DATA_READ					0x30

class AcsiDataTrans
{
public:
    AcsiDataTrans();
    ~AcsiDataTrans();

    void clear(void);

    void setStatus(BYTE stat);
    void addData(BYTE val);
    void addData(BYTE *data, DWORD cnt);

    bool recvData(BYTE *data, DWORD cnt);
    void sendDataAndStatus(void);

    void setCommunicationObject(CConUsb *comIn);

    void getAtnWord(BYTE *bfr);
    void setAtnWord(BYTE *bfr);

private:
    BYTE    *buffer;
    DWORD   count;
    BYTE    status;

    bool    statusWasSet;

    CConUsb *com;

    BYTE    *recvBuffer;

    struct {
        bool got;
        BYTE bytes[2];
    } prevAtnWord;
};

#endif // ACSIDATATRANS_H
