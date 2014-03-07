#ifndef _UPDATE_H
#define _UPDATE_H

#include <map>

#include "utils.h"
#include "global.h"
#include "version.h"

#define UPDATE_REMOTEURL    "http://joo.kie.sk/cosmosex/update/updatelist.csv"
#define UPDATE_LOCALPATH    "update"
#define UPDATE_LOCALLIST    "update/updatelist.csv"
#define UPDATE_SCRIPT       "update/doupdate.sh"
#define UPDATE_APP_PATH     "app"

#define UPDATE_STATE_IDLE           0
#define UPDATE_STATE_DOWNLOADING    1
#define UPDATE_STATE_DOWNLOAD_OK    2
#define UPDATE_STATE_DOWNLOAD_FAIL  3

class Update
{
public:
    static Versions versions;
	
    static void initialize(void);
    static void downloadUpdateList(void);
    static void processUpdateList(void);

    static void downloadNewComponents(void);
    static bool allNewComponentsDownloaded(void);

    static int  state(void);
    static void stateGoIdle(void);

    static bool createUpdateScript(void);

private:
    static void deleteLocalComponent(std::string url);
    static void startComponentDownloadIfNewer(Version &vLocal, Version &vServer);
    static void getLocalPathFromUrl(std::string url, std::string &localPath);
    static bool isUpToDateOrUpdateDownloaded(Version &vLocal, Version &vServer);

    static int currentState;
};

#endif // CCORETHREAD_H
