#ifndef _UPDATE_H
#define _UPDATE_H

#include <map>

#include "global.h"
#include "version.h"

#define UPDATE_LOCALPATH    "/tmp"
#define UPDATE_SCRIPT       "/ce/update/doupdate.sh"
#define UPDATE_APP_PATH     "/ce/app"
#define UPDATE_REBOOT_FILE  "/tmp/REBOOT_AFTER_UPDATE"
#define UPDATE_USBFILE      "/tmp/UPDATE_FROM_USB"

#define REPORT_URL          "http://joo.kie.sk/cosmosex/update/report.php"

class Update
{
public:
    static Versions versions;

    static void initialize(void);

    static bool createFlashFirstFwScript(bool withLinuxRestart);
    static bool createUpdateXilinxScript(void);

    static bool checkForUpdateListOnUsb(std::string &updateFilePath);

    static void createFloppyTestImage(void);
    static bool writeSimpleTextFile(const char *path, const char *content);
    static void removeSimpleTextFile(const char *path);
    static const char *getUsbArchiveName(void);

private:
    static DWORD    whenCanStartInstall;
    static const char *getPropperXilinxTag(void);
};

#endif // CCORETHREAD_H
