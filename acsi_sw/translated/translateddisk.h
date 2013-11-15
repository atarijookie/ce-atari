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

    int         translatedType;             // normal / shared / config

} TranslatedConf;

typedef struct {
    FILE *hostHandle;                       // file handle for all the work with the file on host
    BYTE atariHandle;                       // file handle used on Atari
    std::string hostPath;                   // where is the file on host file system
} TranslatedFiles;

#define MAX_FILES       40                  // maximum open files count, 40 is the value from EmuTOS

#define TRANSLATEDTYPE_NORMAL           0
#define TRANSLATEDTYPE_SHAREDDRIVE      1
#define TRANSLATEDTYPE_CONFIGDRIVE      2

class TranslatedDisk
{
public:
    TranslatedDisk(void);
    ~TranslatedDisk();

    void setAcsiDataTrans(AcsiDataTrans *dt);

    void processCommand(BYTE *cmd);

    bool attachToHostPath(std::string hostRootPath, int translatedType);
    void dettachFromHostPath(std::string hostRootPath);
    void dettachAll(void);
    bool isAlreadyAttached(std::string hostRootPath);

    void configChanged_reload(void);

private:
    AcsiDataTrans   *dataTrans;

    BYTE            *dataBuffer;
    BYTE            *dataBuffer2;

    TranslatedConf  conf[16];               // 16 possible TOS drives
    char            currentDriveLetter;
    BYTE            currentDriveIndex;

    TranslatedFiles files[MAX_FILES];       // open files

    struct {
        int firstTranslated;
        int shared;
        int confDrive;
    } driveLetters;

    struct {
        BYTE *buffer;
        WORD count;             // count of items found

        WORD fsnextStart;
        WORD maxCount;          // maximum count of items that this buffer can hold
    } findStorage;

    void loadSettings(void);

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
    void onFdelete(BYTE *cmd);
    void onFattrib(BYTE *cmd);

    // file content functions -- need file handle
    void onFcreate(BYTE *cmd);
    void onFopen(BYTE *cmd);
    void onFclose(BYTE *cmd);
    void onFdatime(BYTE *cmd);
    void onFread(BYTE *cmd);
    void onFwrite(BYTE *cmd);
    void onFseek(BYTE *cmd);

    // these are not really needed, but we need to handle them so the things don't get messy
    void onFdup(BYTE *cmd);
    void onFforce(BYTE *cmd);

    // date and time function
    void onTgetdate(BYTE *cmd);
    void onTsetdate(BYTE *cmd);
    void onTgettime(BYTE *cmd);
    void onTsettime(BYTE *cmd);

    // custom functions, which are not translated gemdos functions, but needed to do some other work
    void onFtell(BYTE *cmd);


    // helper functions
    void attributesHostToAtari(DWORD attrHost, BYTE &attrAtari);
    void attributesAtariToHost(BYTE attrAtari, DWORD &attrHost);

    WORD fileTimeToAtariDate(FILETIME *ft);
    WORD fileTimeToAtariTime(FILETIME *ft);

    void appendFoundToFindStorage(WIN32_FIND_DATAA *found, unsigned char findAttribs);

    int findEmptyFileSlot(void);
    int findFileHandleSlot(int atariHandle);

    WORD  getWord(BYTE *bfr);
    DWORD getDword(BYTE *bfr);
};

#endif
