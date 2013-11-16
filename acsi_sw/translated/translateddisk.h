#ifndef TRANSLATEDDISK_H
#define TRANSLATEDDISK_H

#include <windows.h>

#include "../acsidatatrans.h"
#include "../datatypes.h"

#define BUFFER_SIZE             (1024*1024)
#define BUFFER_SIZE_SECTORS     (BUFFER_SIZE / 512)

#define HOSTPATH_SEPAR_STRING       "\\"
#define HOSTPATH_SEPAR_CHAR         '\\'
#define ATARIPATH_SEPAR_CHAR        '/'

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
#define MAX_DRIVES      16

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
    void detachFromHostPath(std::string hostRootPath);
    void detachAll(void);

    void configChanged_reload(void);

private:
    AcsiDataTrans   *dataTrans;

    BYTE            *dataBuffer;
    BYTE            *dataBuffer2;

    TranslatedConf  conf[MAX_DRIVES];       // 16 possible TOS drives
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
    void removeDoubleDots(std::string &path);
    void pathSeparatorAtariToHost(std::string &path);
    void pathSeparatorHostToAtari(std::string &path);
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
//  void onFsetdta(BYTE *cmd);                    // this function needs to be handled on ST only
//  void onFgetdta(BYTE *cmd);                    // this function needs to be handled on ST only
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

    // date and time function
    void onTgetdate(BYTE *cmd);
    void onTsetdate(BYTE *cmd);
    void onTgettime(BYTE *cmd);
    void onTsettime(BYTE *cmd);

    // custom functions, which are not translated gemdos functions, but needed to do some other work
    void onInitialize(void);            // this method is called on the startup of CosmosEx translated disk driver
    void onFtell(BYTE *cmd);            // this is needed after Fseek

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

    void closeFileByIndex(int index);
    void closeAllFiles(void);

    void attachToHostPathByIndex(int index, std::string hostRootPath, int translatedType);
    void detachByIndex(int index);
    bool isAlreadyAttached(std::string hostRootPath);
};

#endif
