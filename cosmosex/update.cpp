#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#include "global.h"
#include "debug.h"
#include "settings.h"
#include "downloader.h"
#include "update.h"

Versions Update::versions;

void Update::initialize(void)
{
    Update::versions.current.app.fromString(                (char *) APP_VERSION);
    Update::versions.current.xilinx.fromFirstLineOfFile(    (char *) XILINX_VERSION_FILE);
    Update::versions.current.imglist.fromFirstLineOfFile(   (char *) IMAGELIST_FILE);
    Update::versions.updateListWasProcessed = false;
    Update::versions.gotUpdate              = false;
}

void Update::processUpdateList(void)
{
    // check if the local update list exists
    int res = access(UPDATE_LOCALLIST, F_OK);

    if(res != 0) {                              // local update list doesn't exist, quit for now
        return;
    }

    Debug::out("processUpdateList - starting");

    // open update list, parse Update::versions
    FILE *f = fopen(UPDATE_LOCALLIST, "rt");

    if(!f) {
        Debug::out("processUpdateList - couldn't open file %s", UPDATE_LOCALLIST);
        return;
    }

    char line[1024];
    char what[32], ver[32], url[256], crc[32];
    while(!feof(f)) {
        char *r = fgets(line, 1024, f);                 // read the update Update::versions file by lines

        if(!r) {
            continue;
        }

        // try to separate the sub strings
        res = sscanf(line, "%[^,\n],%[^,\n],%[^,\n],%[^,\n]", what, ver, url, crc);

        if(res != 4) {
            continue;
        }

        // now store the Update::versions where they bellong
        if(strncmp(what, "app", 3) == 0) {
            Update::versions.onServer.app.fromString(ver);
            Update::versions.onServer.app.setUrlAndChecksum(url, crc);
            continue;
        }

        if(strncmp(what, "hans", 4) == 0) {
            Update::versions.onServer.hans.fromString(ver);
            Update::versions.onServer.hans.setUrlAndChecksum(url, crc);
            continue;
        }

        if(strncmp(what, "xilinx", 6) == 0) {
            Update::versions.onServer.xilinx.fromString(ver);
            Update::versions.onServer.xilinx.setUrlAndChecksum(url, crc);
            continue;
        }

        if(strncmp(what, "franz", 5) == 0) {
            Update::versions.onServer.franz.fromString(ver);
            Update::versions.onServer.franz.setUrlAndChecksum(url, crc);
            continue;
        }

        if(strncmp(what, "imglist", 7) == 0) {
            Update::versions.onServer.imglist.fromString(ver);
            Update::versions.onServer.imglist.setUrlAndChecksum(url, crc);
            continue;
        }
    }

    fclose(f);

    //-------------------
    // now compare Update::versions - current with those on server, if anything new then set a flag
    if(Update::versions.current.app.isOlderThan( Update::versions.onServer.app )) {
        Update::versions.gotUpdate = true;
        Debug::out("processUpdateList - APP is newer on server");
    }

    if(Update::versions.current.hans.isOlderThan( Update::versions.onServer.hans )) {
        Update::versions.gotUpdate = true;
        Debug::out("processUpdateList - HANS is newer on server");
    }

    if(Update::versions.current.xilinx.isOlderThan( Update::versions.onServer.xilinx )) {
        Update::versions.gotUpdate = true;
        Debug::out("processUpdateList - XILINX is newer on server");
    }

    if(Update::versions.current.franz.isOlderThan( Update::versions.onServer.franz )) {
        Update::versions.gotUpdate = true;
        Debug::out("processUpdateList - FRANZ is newer on server");
    }

    // check this one and if we got an update, do a silent update 
    if(Update::versions.current.imglist.isOlderThan( Update::versions.onServer.imglist )) {
        Debug::out("processUpdateList - IMAGE LIST is newer on server, doing silent update...");

        TDownloadRequest tdr;
        tdr.srcUrl = Update::versions.onServer.imglist.getUrl();
        tdr.dstDir = "";
        downloadAdd(tdr);
    }

    Update::versions.updateListWasProcessed = true;         // mark that the update list was processed and don't need to do this again
    Debug::out("processUpdateList - done");
}

void Update::downloadUpdateList(void)
{
    // check for existence and possibly create update dir
  	int res = mkdir(UPDATE_LOCALPATH, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);		// mod: 0x775
	
	if(res == 0) {					// dir created
		Debug::out("Update: directory %s was created.", UPDATE_LOCALPATH);
	} else {						// dir not created
		if(errno != EEXIST) {		// and it's not because it already exists...
			Debug::out("Update: failed to create settings directory - %s", strerror(errno));
		}
	}

    // remove old update list
    remove(UPDATE_LOCALLIST);

    // add request for download of the update list
    TDownloadRequest tdr;
    tdr.srcUrl = UPDATE_REMOTEURL;
    tdr.dstDir = UPDATE_LOCALPATH;
    downloadAdd(tdr);
}

void Update::downloadNewComponents(void)
{
    // if the list wasn't processed, don't do anything
    if(!Update::versions.updateListWasProcessed) {              
        return;
    }

    // delete local versions of components (which might be already downloaded)
    deleteLocalComponent(Update::versions.onServer.app.getUrl());
    deleteLocalComponent(Update::versions.onServer.hans.getUrl());
    deleteLocalComponent(Update::versions.onServer.xilinx.getUrl());
    deleteLocalComponent(Update::versions.onServer.franz.getUrl());

    // start the download of newer components
    startComponentDownloadIfNewer(Update::versions.current.app,     Update::versions.onServer.app);
    startComponentDownloadIfNewer(Update::versions.current.hans,    Update::versions.onServer.hans);
    startComponentDownloadIfNewer(Update::versions.current.xilinx,  Update::versions.onServer.xilinx);
    startComponentDownloadIfNewer(Update::versions.current.franz,   Update::versions.onServer.franz);
}

bool Update::allNewComponentsDownloaded(void)
{
    int a,b,c,d;

    // check if the components are either up to date, or downloaded
    a = isUpToDateOrUpdateDownloaded(Update::versions.current.app,     Update::versions.onServer.app);
    b = isUpToDateOrUpdateDownloaded(Update::versions.current.hans,    Update::versions.onServer.hans);
    c = isUpToDateOrUpdateDownloaded(Update::versions.current.xilinx,  Update::versions.onServer.xilinx);
    d = isUpToDateOrUpdateDownloaded(Update::versions.current.franz,   Update::versions.onServer.franz);

    if(a && b && c && d) {      // everything up to date or downloaded? 
        return true;
    }

    return false;               // not everything up to date or downloaded
}

bool Update::isUpToDateOrUpdateDownloaded(Version &vLocal, Version &vServer)
{
    // if local is not older than on server, we're up to date, don't wait for this file
    if(!vLocal.isOlderThan(vServer)) {          
        return true;
    }

    std::string localFile;
    getLocalPathFromUrl(vServer.getUrl(), localFile);       // convert url to local path

    int res = access(localFile.c_str(), F_OK);              // check if file exists

    if(res == 0) {                                          // file exists, so it's downloaded
        return true;
    }

    return false;                                           // file doesn't exist
}

void Update::deleteLocalComponent(std::string url)
{
    std::string finalFile;

    getLocalPathFromUrl(url, finalFile);                    // convert remove path to local path
    remove(finalFile.c_str());                              // try to delete it, don't care if it succeeded (it might not exist)
}

void Update::startComponentDownloadIfNewer(Version &vLocal, Version &vServer)
{
    // if local is not older than on server, quit
    if(!vLocal.isOlderThan(vServer)) {          
        return;
    }

    // start the download
    TDownloadRequest tdr;
    tdr.srcUrl = vServer.getUrl();
    tdr.dstDir = UPDATE_LOCALPATH;
    downloadAdd(tdr);
}

void Update::getLocalPathFromUrl(std::string url, std::string &localPath)
{
    std::string urlPath, fileName, finalFile;
    Utils::splitFilenameFromPath(url, urlPath, fileName);   // get just the filename from url
    
    finalFile = UPDATE_LOCALPATH;
    Utils::mergeHostPaths(finalFile, fileName);             // create final local filename with path

    localPath = finalFile;
}


