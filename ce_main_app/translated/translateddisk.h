#ifndef TRANSLATEDDISK_H
#define TRANSLATEDDISK_H

#include <stdio.h>
#include <string>
#include <time.h>

#include "translatedhelper.h"
#include "dirtranslator.h"

#include "../acsidatatrans.h"
#include "../datatypes.h"

#include "../isettingsuser.h"

#include "service/configservice.h"
#include "service/screencastservice.h"
#include "acsicommand/dateacsicommand.h"
#include "acsicommand/screencastacsicommand.h"

#include "../settingsreloadproxy.h"

#define CONFIG_DRIVE_PATH       "/tmp/configdrive"
#define SHARED_DRIVE_PATH       "/mnt/shared"

// this version number should be increased whenever command, data or status part of translated disk has changed - to enable CE_DD vs. Main App pairing. 
// The CE_DD should check this version number and it this doesn't match expectations, it should refuse to work.
#define TRANSLATEDDISK_VERSION  0x0101

typedef struct {
    bool        enabled;
    bool        mediaChanged;

    std::string hostRootPath;               // where is the root on host file system
    char        stDriveLetter;              // what letter will be used on ST
    std::string currentAtariPath;           // what is the current path on this drive

    int         translatedType;             // normal / shared / config

	DirTranslator	dirTranslator;			// used for translating long dir / file names to short ones
} TranslatedConf;

typedef struct {
    bool        enabled;
    std::string hostRootPath;               // where is the root on host file system
    int         translatedType;             // normal / shared / config
} TranslatedConfTemp;

typedef struct {
    FILE *hostHandle;                       // file handle for all the work with the file on host
    BYTE atariHandle;                       // file handle used on Atari
    std::string hostPath;                   // where is the file on host file system

    DWORD lastDataCount;                    // stores the data count that got on the last read / write operation
} TranslatedFiles;

#define MAX_FILES       40                  // maximum open files count, 40 is the value from EmuTOS
#define MAX_DRIVES      16

#define TRANSLATEDTYPE_NORMAL           0
#define TRANSLATEDTYPE_SHAREDDRIVE      1
#define TRANSLATEDTYPE_CONFIGDRIVE      2

#define MAX_FIND_STORAGES               32

//---------------------------------------
#define MAX_ZIP_DIRS                    5

#define ZIPDIR_PATH_PREFIX              "/tmp/zipdir"
#define ZIPDIR_PATH_LENGTH              12              // length of ZIP DIR path, including the number which is generated in getZipDirMountPoint()

//---------------------------------------
// Pexec() image stuff
// whole image size
#define PEXEC_DRIVE_SIZE_BYTES          (5 * 1024 * 1024)
#define PEXEC_DRIVE_SIZE_SECTORS        (PEXEC_DRIVE_SIZE_BYTES / 512)

// FAT size
#define PEXEC_FAT_BYTES_NEEDED          (PEXEC_DRIVE_SIZE_SECTORS * 2)
#define PEXEC_FAT_SECTORS_NEEDED        (PEXEC_FAT_BYTES_NEEDED / 512)

// image size usable for data = whole image size - 2 * fat size
#define PEXEC_DRIVE_USABLE_SIZE_SECTORS (PEXEC_DRIVE_SIZE_SECTORS - (2 * PEXEC_FAT_SECTORS_NEEDED) - 10)
#define PEXEC_DRIVE_USABLE_SIZE_BYTES   (PEXEC_DRIVE_USABLE_SIZE_SECTORS * 512)

//---------------------------------------

class ZipDirEntry {
    public:

    ZipDirEntry(int index) {
        clear(index);
    }
    
    std::string realHostPath;           // real path to ZIP file on host dir struct, e.g. /mnt/shared/normal/archive.zip
    std::string mountPoint;             // contains path, where the ZIP file will be mounted, and where createHostPath() will redirect host path
    DWORD       lastAccessTime;         // Utils::getCurrentMs() time which is stored on each access to this folder - will be used to unmount oldest ZIP files, so new ZIP files can be mounted / accessed
    
    int         mountActionStateId;     // this is TMountActionState.id, which you can use to find out if the mount action did already finish
    bool        isMounted;              // if this is set to true, don't have to check mount action state, but just consider this to be mounted
    
    void clear(int index) {
        realHostPath        = "";
        lastAccessTime      = 0;
        mountActionStateId  = 0;
        
        getZipDirMountPoint(index, mountPoint);
    }
    
    void getZipDirMountPoint(int index, std::string &aMountPoint)
    {
        char indexNoStr[128];
        sprintf(indexNoStr, "%d", index);       // generate mount point number string, e.g. "5"
        
        aMountPoint  = ZIPDIR_PATH_PREFIX;      // e.g. /tmp/zipdir
        aMountPoint += indexNoStr;
    }
};

class TranslatedDisk: public ISettingsUser
{
public:
    TranslatedDisk(AcsiDataTrans *dt, ConfigService *cs, ScreencastService *scs);
    virtual ~TranslatedDisk();

    void processCommand(BYTE *cmd);

    bool attachToHostPath(std::string hostRootPath, int translatedType);
    void detachFromHostPath(std::string hostRootPath);
    void detachAll(void);
    void detachAllUsbMedia(void);

    virtual void reloadSettings(int type);      // from ISettingsUser
    
    void setSettingsReloadProxy(SettingsReloadProxy *rp);

    bool hostPathExists(std::string hostPath);
    void pathSeparatorAtariToHost(std::string &path);
    
    bool createFullAtariPathAndFullHostPath(std::string inPartialAtariPath, std::string &outFullAtariPath, int &outAtariDriveIndex, std::string &outFullHostPath, bool &waitingForMount, int &zipDirNestingLevel);

private:
	void mountAndAttachSharedDrive(void);
	void attachConfigDrive(void);

    AcsiDataTrans       *dataTrans;
    SettingsReloadProxy *reloadProxy;

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

        WORD readOnly;
    } driveLetters;

    char asciiAtariToPc[256];

	TFindStorage tempFindStorage;
    TFindStorage *findStorages[MAX_FIND_STORAGES];

    void loadSettings(void);

    WORD getDrivesBitmap(void);
    bool isDriveIndexReadOnly(int driveIndex);
    void removeDoubleDots(std::string &path);
    void pathSeparatorHostToAtari(std::string &path);
    bool isLetter(char a);
    char toUpperCase(char a);
    bool isValidDriveLetter(char a);
    bool pathContainsWildCards(char *path);
    int  deleteDirectoryPlain(char *path);

    bool startsWith(std::string what, std::string subStr);
    bool endsWith(std::string what, std::string subStr);

    void onGetConfig(BYTE *cmd);
    void getIpAdds(BYTE *bfr);

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
    
    // Pexec() handling
    void onPexec(BYTE *cmd);

    // custom functions, which are not translated gemdos functions, but needed to do some other work
    void onInitialize(void);            // this method is called on the startup of CosmosEx translated disk driver
    void onFtell(BYTE *cmd);            // this is needed after Fseek
    void onRWDataCount(BYTE *cmd);      // when Fread / Fwrite doesn't process all the data, this returns the count of processed data
    void onFsnext_last(BYTE *cmd);      // after last Fsnext() call this to release the findStorage
    void getByteCountToEndOfFile(BYTE *cmd);    // should be used with Fread() to know the exact count of bytes to the end of file, so the memory after the last valid byte won't get corrupted

    // BIOS functions we need to support
    void onDrvMap(BYTE *cmd);
    void onMediach(BYTE *cmd);
    void onGetbpb(BYTE *cmd);

	// other functions
	void onGetMounts(BYTE *cmd);
    void onUnmountDrive(BYTE *cmd);
    void onStLog(BYTE *cmd);
    void onTestRead(BYTE *cmd);
    void onTestWrite(BYTE *cmd);
    void getScreenShotConfig(BYTE *cmd);
    
    // helper functions
    int findEmptyFileSlot(void);
    int findFileHandleSlot(int atariHandle);

    void closeFileByIndex(int index);
    void closeAllFiles(void);

    void attachToHostPathByIndex(int index, std::string hostRootPath, int translatedType);
    void detachByIndex(int index);
    bool isAlreadyAttached(std::string hostRootPath);

    char *functionCodeToName(int code);
    void atariFindAttribsToString(BYTE attr, std::string &out);
    bool isRootDir(std::string hostPath);

    void initAsciiTranslationTable(void);
    void convertAtariASCIItoPc(char *path);

    DWORD getByteCountToEOF(FILE *f);
    
    int  driveLetterToDriveIndex(char pathDriveLetter);
    
    bool createFullAtariPath(std::string inPartialAtariPath, std::string &outFullAtariPath, int &outAtariDriveIndex);
    void createFullHostPath (std::string inFullAtariPath, int inAtariDriveIndex, std::string &outFullHostPath, bool &waitingForMount, int &zipDirNestingLevel);

    //-----------------------------------
    // ZIP DIR stuff
    ZipDirEntry *zipDirs[MAX_ZIP_DIRS];

    bool useZipdirNotFile;
    
    void getZipDirMountPoint(int index, std::string &mountPoint);
    int  getZipDirByMountPoint(std::string &searchedMountPoint);
    bool zipDirAlreadyMounted(char *zipFile, int &zipDirIndex);
    
    bool isOkToMountThisAsZipDir(char *zipFilePath);
    void doZipDirMountOrStateCheck(bool isMounted, char *zipFilePath, int zipDirIndex, bool &waitingForMount);
    
    void replaceHostPathWithZipDirPath(int inAtariDriveIndex, std::string &hostPath, bool &waitingForMount, int &zipDirNestingLevel);
    void replaceHostPathWithZipDirPath_internal(std::string &hostPath, bool &waitingForMount, bool &containsZip);
    //-----------------------------------
    // helpers for find storage
    void initFindStorages(void);
    void clearFindStorages(void);
    void destroyFindStorages(void);

    int  getEmptyFindStorageIndex(void);
    int  getFindStorageIndexByDta(DWORD dta);

    //-----------------------------------
    // helpers for Pexec()
    void onPexec_createImage(BYTE *cmd);
    void createImage(std::string &fullAtariPath, FILE *f, int fileSize);
    void createDirEntry(bool isRoot, bool isDir, WORD date, WORD time, DWORD fileSize, char *dirEntryName, DWORD thisSectorNo);
    void storeIntelWord (BYTE *p,  WORD a);
    void storeIntelDword(BYTE *p, DWORD a);

    void onPexec_getBpb(BYTE *cmd);
    void onPexec_readSector(BYTE *cmd);

    BYTE *pexecImage;
    //-----------------------------------
    // other ACSI command helpers
    ConfigService*          configService;
    ScreencastService*      screencastService;
    DateAcsiCommand*        dateAcsiCommand;
    ScreencastAcsiCommand*  screencastAcsiCommand;
};

#endif
