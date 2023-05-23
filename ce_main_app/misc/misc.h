#ifndef __MISC_H__
#define __MISC_H__

#ifndef HOSTMOD_MISC
    #define HOSTMOD_MISC        7
#endif

#define MISC_CMD_IDENTIFY       0
#define MISC_CMD_SEND_SERIAL    1
#define MISC_CMD_GET_SETTINGS   2

#define MISC_CMD_GET_UPDATE     10

#define MISC_CMD_HOST_SHUTDOWN  250

class AcsiDataTrans;

class Misc
{
public:
    Misc();
    ~Misc();

    void setDataTrans(AcsiDataTrans *dt);
    void processCommand(uint8_t *cmd);

private:
    uint8_t dataBuffer[512];

    AcsiDataTrans *dataTrans;
    const char *functionCodeToName(int code);

    void recvHwSerial(uint8_t *cmd);
    void getLicense(uint8_t *cmd);
    void getSettings(uint8_t *cmd);
    void getUpdate(uint8_t *cmd);
    void hostShutdown(uint8_t *cmd);
};

#endif
