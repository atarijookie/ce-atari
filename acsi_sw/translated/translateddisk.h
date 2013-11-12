#ifndef TRANSLATEDDISK_H
#define TRANSLATEDDISK_H

#include "../acsidatatrans.h"
#include "datatypes.h"

typedef struct {
    bool        enabled;

    std::string hostPath;                   // where is the root on host file system
    char        stDriveLetter;              // what letter will be used on ST
    std::string currentPath;                // what is the current path on this drive

} TranslatedConf;


class TranslatedDisk
{
public:
    TranslatedDisk(void);
    ~TranslatedDisk();

    void setAcsiDataTrans(AcsiDataTrans *dt);

    void processCommand(BYTE *cmd);

private:
    AcsiDataTrans   *dataTrans;

    BYTE            *dataBuffer;
    BYTE            *dataBuffer2;

    TranslatedConf  conf[16];               // 16 possible TOS drives
    char            currentDriveLetter;
    BYTE            currentDriveIndex;


    WORD getDrivesBitmap(void);

    void onGetConfig(BYTE *cmd);

    // path functions
    void onDsetdrv(BYTE *cmd);
    void onDgetdrv(BYTE *cmd);
    void onDsetpath(BYTE *cmd);
    void onDgetpath(BYTE *cmd);

    // directory & file search
    void onFsetdta(BYTE *cmd);
    void onFgetdta(BYTE *cmd);
    void onFsfirst(BYTE *cmd);
    void onFsnext(BYTE *cmd);

    // file and directory manipulation
    void onDfree(BYTE *cmd);
    void onDcreate(BYTE *cmd);
    void onDdelete(BYTE *cmd);
    void onFrename(BYTE *cmd);
    void onFdatime(BYTE *cmd);
    void onFdelete(BYTE *cmd);
    void onFattrib(BYTE *cmd);

    // file content functions
    void onFcreate(BYTE *cmd);
    void onFopen(BYTE *cmd);
    void onFclose(BYTE *cmd);
    void onFread(BYTE *cmd);
    void onFwrite(BYTE *cmd);
    void onFseek(BYTE *cmd);

    // date and time function
    void onTgetdate(BYTE *cmd);
    void onTsetdate(BYTE *cmd);
    void onTgettime(BYTE *cmd);
    void onTsettime(BYTE *cmd);
};

#endif
