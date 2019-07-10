#ifndef _UPDATE_H
#define _UPDATE_H

#include <map>

#include "global.h"
#include "version.h"

#define UPDATE_REMOTEURL    "http://joo.kie.sk/cosmosex/update/updatelist.csv"
#define UPDATE_LOCALPATH    "/tmp"
#define UPDATE_LOCALLIST    "/tmp/updatelist.csv"
#define UPDATE_SCRIPT       "/ce/update/doupdate.sh"
#define UPDATE_APP_PATH     "/ce/app"

#define UPDATE_USBFILE      "ce_update.zip"

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

private:
    static DWORD    whenCanStartInstall;
    static const char *getPropperXilinxTag(void);
};

#endif // CCORETHREAD_H
