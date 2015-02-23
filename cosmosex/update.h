#ifndef _UPDATE_H
#define _UPDATE_H

#include <map>

#include "utils.h"
#include "global.h"
#include "version.h"

#define UPDATE_REMOTEURL    "http://joo.kie.sk/cosmosex/update/updatelist.csv"
#define UPDATE_LOCALPATH    "/ce/update"
#define UPDATE_LOCALLIST    "/ce/update/updatelist.csv"
#define UPDATE_SCRIPT       "/ce/update/doupdate.sh"
#define UPDATE_APP_PATH     "/ce/app"

#define UPDATE_USBFILE      "ce_update.zip"

#define UPDATE_STATE_IDLE           0
#define UPDATE_STATE_DOWNLOADING    1
#define UPDATE_STATE_DOWNLOAD_OK    2
#define UPDATE_STATE_DOWNLOAD_FAIL  3

class Update
{
public:
    static Versions versions;
	
    static void initialize(void);
    static void deleteLocalUpdateComponents(void);
    static void downloadUpdateList(char *remoteUrl);
    static void processUpdateList(void);

    static void downloadNewComponents(void);
    static bool allNewComponentsDownloaded(void);

    static int  state(void);
    static void stateGoIdle(void);
    static void stateGoDownloadOK(void);

    static bool createUpdateScript(void);
    static bool createFlashFirstFwScript(void);
    
    static bool checkForUpdateListOnUsb(std::string &updateFilePath);

    static bool allDownloadedOk(void);
    static bool someDownloadFailed(void);
    
private:
    static void deleteLocalComponent(std::string url);
    static void startComponentDownloadIfNewer(Version &vLocal, Version &vServer);
    static void getLocalPathFromUrl(std::string url, std::string &localPath);
    static bool isUpToDateOrUpdateDownloaded(Version &vLocal, Version &vServer);
    
    static int currentState;
    
    static const char *getPropperXilinxTag(void);
};

#endif // CCORETHREAD_H
