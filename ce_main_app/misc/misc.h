#ifndef __MISC_H__
#define __MISC_H__

#ifndef HOSTMOD_MISC
    #define HOSTMOD_MISC        7
#endif

#define MISC_CMD_IDENTIFY       0
#define MISC_CMD_SEND_SERIAL    1
#define MISC_CMD_GET_LICENSE    2
#define MISC_CMD_GET_SETTINGS   3

#define MISC_CMD_GET_UPDATE     10

#define MISC_CMD_HOST_SHUTDOWN  250

class AcsiDataTrans;

class Misc
{
public:
    Misc();
    ~Misc();

    void setDataTrans(AcsiDataTrans *dt);
    void processCommand(BYTE *cmd);

private:
    BYTE dataBuffer[512];

    AcsiDataTrans *dataTrans;
    const char *functionCodeToName(int code);

    void recvHwSerialAndDeleteLicense(BYTE *cmd);
    void getLicense(BYTE *cmd);
    void getSettings(BYTE *cmd);
    void getUpdate(BYTE *cmd);
    void hostShutdown(BYTE *cmd);

    void generateLicenseKeyName(char *keyName);
    bool getLicenseForSerialFromSettings(BYTE *bfrLicense);
    void retrieveLicenseForSerial(void);
};

#endif
