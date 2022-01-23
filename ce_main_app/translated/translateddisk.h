// vim: expandtab shiftwidth=4 tabstop=4
#ifndef TRANSLATEDDISK_H
#define TRANSLATEDDISK_H

#include <stdio.h>
#include <string>
#include <time.h>

#include "dirtranslator.h"
#include "../isettingsuser.h"
#include "../settings.h"

class ConfigService;
class ScreencastService;
class DateAcsiCommand;
class ScreencastAcsiCommand;
class AcsiDataTrans;
class SettingsReloadProxy;

#define CONFIG_DRIVE_PATH       "/tmp/configdrive"
#define SHARED_DRIVE_PATH       "/mnt/shared"

// this version number should be increased whenever command, data or status part of translated disk has changed - to enable CE_DD vs. Main App pairing.
// The CE_DD should check this version number and it this doesn't match expectations, it should refuse to work.
#define TRANSLATEDDISK_VERSION  0x0101

typedef struct {
    bool        enabled;
    bool        mediaChanged;

    std::string devicePath;                 // where the device is
    std::string hostRootPath;               // where is the root on host file system
    std::string label;						// "DOS" label
    char        stDriveLetter;              // what letter will be used on ST
    std::string currentAtariPath;           // what is the current path on this drive

    int         translatedType;             // normal / shared / config

	DirTranslator	dirTranslator;			// used for translating long dir / file names to short ones
} TranslatedConf;

typedef struct {
    bool        enabled;
    std::string devicePath;                 // where the device is
    std::string hostRootPath;               // where is the root on host file system
    std::string label;						// "DOS" label
    int         translatedType;             // normal / shared / config
} TranslatedConfTemp;

typedef struct {
    FILE *hostHandle;                       // file handle for all the work with the file on host
    uint8_t atariHandle;                       // file handle used on Atari
    std::string hostPath;                   // where is the file on host file system

    uint32_t lastDataCount;                    // stores the data count that got on the last read / write operation
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
    uint32_t       lastAccessTime;         // Utils::getCurrentMs() time which is stored on each access to this folder - will be used to unmount oldest ZIP files, so new ZIP files can be mounted / accessed

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
private:
    static TranslatedDisk * instance;
    static pthread_mutex_t mutex;
    TranslatedDisk(AcsiDataTrans *dt);
    virtual ~TranslatedDisk();

public:
    static TranslatedDisk * createInstance(AcsiDataTrans *dt);
    static TranslatedDisk * getInstance(void);
    static void deleteInstance(void);

    void mutexLock(void);
    void mutexUnlock(void);

    void processCommand(uint8_t *cmd);

    bool attachToHostPath(std::string hostRootPath, int translatedType, std::string devicePath);
    void detachFromHostPath(std::string hostRootPath);
    void detachAll(void);
    void detachAllUsbMedia(void);

    virtual void reloadSettings(int type);      // from ISettingsUser

    void setSettingsReloadProxy(SettingsReloadProxy *rp);

    bool hostPathExists(std::string &hostPath, bool alsoCheckCaseInsensitive = false);
    bool hostPathExists_caseInsensitive(std::string hostPath, std::string &justPath, std::string &originalFileName, std::string &foundFileName);
    void updateDirTranslators(std::string hostPath, std::string oldFileName, std::string newFileName);

    static void pathSeparatorAtariToHost(std::string &path);

    bool createFullAtariPathAndFullHostPath(const std::string &inPartialAtariPath, std::string &outFullAtariPath, int &outAtariDriveIndex, std::string &outFullHostPath, bool &waitingForMount, int &zipDirNestingLevel);
    void createFullHostPath (const std::string &inFullAtariPath, int inAtariDriveIndex, std::string &outFullHostPath, bool &waitingForMount, int &zipDirNestingLevel);

    bool getPathToUsbDriveOrSharedDrive(std::string &hostRootPath);

    // for status report
    bool driveIsEnabled(int driveIndex);
    void driveGetReport(int driveIndex, std::string &reportString);
	const char * driveGetHostPath(int driveIndex) const {
		if(!conf[driveIndex].enabled) return NULL;
		return conf[driveIndex].hostRootPath.c_str();
	}

    void fillDisplayLines(void);

private:
	void mountAndAttachSharedDrive(void);
	void attachConfigDrive(void);

    AcsiDataTrans       *dataTrans;
    SettingsReloadProxy *reloadProxy;

    uint8_t            *dataBuffer;
    uint8_t            *dataBuffer2;

    TranslatedConf  conf[MAX_DRIVES];       // 16 possible TOS drives
    char            currentDriveLetter;
    uint8_t            currentDriveIndex;

    TranslatedFiles files[MAX_FILES];       // open files

    struct {
        int firstTranslated;
        int shared;
        int confDrive;

        uint16_t readOnly;
    } driveLetters;

    char asciiAtariToPc[256];

	TFindStorage tempFindStorage;
    TFindStorage *findStorages[MAX_FIND_STORAGES];

    void loadSettings(void);

    uint16_t getDrivesBitmap(void);
    bool isDriveIndexReadOnly(int driveIndex);
    static void removeDoubleDots(std::string &path);
    static void pathSeparatorHostToAtari(std::string &path);
    static bool isLetter(char a);
    static char toUpperCase(char a);
    static bool isValidDriveLetter(char a);
    static bool pathContainsWildCards(const char *path);
    static int  deleteDirectoryPlain(const char *path);

    static bool startsWith(std::string what, std::string subStr);
    static bool endsWith(std::string what, std::string subStr);

    void onGetConfig(uint8_t *cmd);
    void getIpAdds(uint8_t *bfr);

    // path functions
    void onDsetdrv(uint8_t *cmd);
    void onDgetdrv(uint8_t *cmd);
    void onDsetpath(uint8_t *cmd);
    void onDgetpath(uint8_t *cmd);

    // directory & file search
//  void onFsetdta(uint8_t *cmd);                    // this function needs to be handled on ST only
//  void onFgetdta(uint8_t *cmd);                    // this function needs to be handled on ST only
    void onFsfirst(uint8_t *cmd);
    void onFsnext(uint8_t *cmd);

    // file and directory manipulation
    void onDfree(uint8_t *cmd);
    void onDcreate(uint8_t *cmd);
    void onDdelete(uint8_t *cmd);
    void onFrename(uint8_t *cmd);
    void onFdelete(uint8_t *cmd);
    void onFattrib(uint8_t *cmd);

    // file content functions -- need file handle
    void onFcreate(uint8_t *cmd);
    void onFopen(uint8_t *cmd);
    void onFclose(uint8_t *cmd);
    void onFdatime(uint8_t *cmd);
    void onFread(uint8_t *cmd);
    void onFwrite(uint8_t *cmd);
    void onFseek(uint8_t *cmd);

    // Pexec() handling
    void onPexec(uint8_t *cmd);

    // custom functions, which are not translated gemdos functions, but needed to do some other work
    void onInitialize(void);            // this method is called on the startup of CosmosEx translated disk driver
    void onFtell(uint8_t *cmd);            // this is needed after Fseek
    void onRWDataCount(uint8_t *cmd);      // when Fread / Fwrite doesn't process all the data, this returns the count of processed data
    void onFsnext_last(uint8_t *cmd);      // after last Fsnext() call this to release the findStorage
    void getByteCountToEndOfFile(uint8_t *cmd);    // should be used with Fread() to know the exact count of bytes to the end of file, so the memory after the last valid byte won't get corrupted

    // BIOS functions we need to support
    void onDrvMap(uint8_t *cmd);
    void onMediach(uint8_t *cmd);
    void onGetbpb(uint8_t *cmd);

	// other functions
	void onGetMounts(uint8_t *cmd);
    void onUnmountDrive(uint8_t *cmd);
    void onStLog(uint8_t *cmd);
    void onStHttp(uint8_t *cmd);
    void onTestRead(uint8_t *cmd);
    void onTestWrite(uint8_t *cmd);
    void onTestGetACSIids(uint8_t *cmd);

    void onSetACSIids(uint8_t *cmd);
    int  findCurrentIDforDevType(int devType, AcsiIDinfo *aii);

    void getScreenShotConfig(uint8_t *cmd);

    // helper functions
    int findEmptyFileSlot(void);
    int findFileHandleSlot(int atariHandle);

    void closeFileByIndex(int index);
    void closeAllFiles(void);

    void attachToHostPathByIndex(int index, std::string hostRootPath, int translatedType, std::string devicePath);
    void detachByIndex(int index);
    bool isAlreadyAttached(std::string hostRootPath);

    const char *functionCodeToName(int code);
    void atariFindAttribsToString(uint8_t attr, std::string &out);
    bool isRootDir(std::string hostPath);

    void initAsciiTranslationTable(void);
    void convertAtariASCIItoPc(char *path);

    uint32_t getByteCountToEOF(FILE *f);

    int  driveLetterToDriveIndex(char pathDriveLetter);

    bool createFullAtariPath(std::string inPartialAtariPath, std::string &outFullAtariPath, int &outAtariDriveIndex);

    //-----------------------------------
    // ZIP DIR stuff
    ZipDirEntry *zipDirs[MAX_ZIP_DIRS];

    bool useZipdirNotFile;

    void getZipDirMountPoint(int index, std::string &mountPoint);
    int  getZipDirByMountPoint(std::string &searchedMountPoint);
    bool zipDirAlreadyMounted(const char *zipFile, int &zipDirIndex);

    static bool isOkToMountThisAsZipDir(const char *zipFilePath);
    void doZipDirMountOrStateCheck(bool isMounted, char *zipFilePath, int zipDirIndex, bool &waitingForMount);

    void replaceHostPathWithZipDirPath(int inAtariDriveIndex, std::string &hostPath, bool &waitingForMount, int &zipDirNestingLevel);
    void replaceHostPathWithZipDirPath_internal(std::string &hostPath, bool &waitingForMount, bool &containsZip);
    //-----------------------------------
    // helpers for find storage
    void initFindStorages(void);
    void clearFindStorages(void);
    void destroyFindStorages(void);

    int  getEmptyFindStorageIndex(void);
    int  getFindStorageIndexByDta(uint32_t dta);

    //-----------------------------------
    // helpers for Pexec()
    void onPexec_createImage(uint8_t *cmd);
    void createImage(std::string &fullAtariPath, FILE *f, int fileSizeBytes, uint16_t atariTime, uint16_t atariDate);
    void createDirEntry(bool isRoot, bool isDir, uint16_t date, uint16_t time, uint32_t fileSize, const char *dirEntryName, int sectorNoAbs, int sectorNoRel);
    void storeDirEntry(uint8_t *pEntry, const char *dirEntryName, bool isDir, uint16_t time, uint16_t date, uint16_t startingSector, uint32_t entrySizeBytes);
    void storeFatChain(uint8_t *pbFat, uint16_t sectorStart, uint16_t sectorEnd);
    void storeIntelWord (uint8_t *p,  uint16_t a);
    void storeIntelDword(uint8_t *p, uint32_t a);

    void onPexec_getBpb(uint8_t *cmd);
    void onPexec_readSector(uint8_t *cmd);
    void onPexec_writeSector(uint8_t *cmd);

    bool pexecWholeFileWasRead(void);

    uint16_t prgSectorStart;
    uint16_t prgSectorEnd;
    int  pexecDriveIndex;

    std::string pexecPrgPath;
    std::string pexecPrgFilename;
    std::string pexecFakeRootPath;

    uint8_t *pexecImage;
    uint8_t *pexecImageReadFlags;
    //-----------------------------------
    // other ACSI command helpers
    ConfigService*          configService;
    ScreencastService*      screencastService;
    DateAcsiCommand*        dateAcsiCommand;
    ScreencastAcsiCommand*  screencastAcsiCommand;
};

#endif
