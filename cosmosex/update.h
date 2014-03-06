#ifndef _UPDATE_H
#define _UPDATE_H

#include <map>

#include "utils.h"
#include "global.h"
#include "version.h"

class Update
{
public:
    static Versions versions;
	
    static void initialize(void);
    static void downloadUpdateList(void);
    static void processUpdateList(void);

    static void downloadNewComponents(void);
    static bool allNewComponentsDownloaded(void);

private:
    static void deleteLocalComponent(std::string url);
    static void startComponentDownloadIfNewer(Version &vLocal, Version &vServer);
    static void getLocalPathFromUrl(std::string url, std::string &localPath);
    static bool isUpToDateOrUpdateDownloaded(Version &vLocal, Version &vServer);

};

#endif // CCORETHREAD_H
