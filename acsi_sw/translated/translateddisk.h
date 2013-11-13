#ifndef TRANSLATEDDISK_H
#define TRANSLATEDDISK_H

#include "../acsidatatrans.h"
#include "datatypes.h"

#define BUFFER_SIZE             (1024*1024)
#define BUFFER_SIZE_SECTORS     (BUFFER_SIZE / 512)

typedef struct {
    bool        enabled;

    std::string hostRootPath;               // where is the root on host file system
    char        stDriveLetter;              // what letter will be used on ST
    std::string currentAtariPath;           // what is the current path on this drive

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
    bool hostPathExists(std::string hostPath);
    bool createHostPath(std::string atariPath, std::string &hostPath);
    void createAtariPathFromHostPath(std::string hostPath, std::string &atariPath);
    bool newPathRequiresCurrentDriveChange(std::string atariPath, int &newDriveIndex);
    bool isLetter(char a);
    char toUpperCase(char a);
    bool isValidDriveLetter(char a);

    bool startsWith(std::string what, std::string subStr);
    bool endsWith(std::string what, std::string subStr);

    void onGetConfig(BYTE *cmd);

    // path functions
    void onDsetdrv(BYTE *cmd);
    void onDgetdrv(BYTE *cmd);
    void onDsetpath(BYTE *cmd);
    void onDgetpath(BYTE *cmd);

    // directory & file search
//    void onFsetdta(BYTE *cmd);                    // this function needs to be handled on ST only
//    void onFgetdta(BYTE *cmd);                    // this function needs to be handled on ST only
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
