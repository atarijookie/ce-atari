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
int Update::currentState = UPDATE_STATE_IDLE;

extern int hwVersion;           // HW version is 1 or 2, and in other cases defaults to 1
extern int hwHddIface;          // HDD interface is either SCSI (HDD_IF_SCSI), or defaults to ACSI (HDD_IF_ACSI)

void Update::initialize(void)
{
    char appVersion[16];
    Version::getAppVersion(appVersion);
    
    Update::versions.current.app.fromString(                (char *) appVersion);
    Update::versions.current.xilinx.fromFirstLineOfFile(    (char *) XILINX_VERSION_FILE);
    Update::versions.updateListWasProcessed = false;
    Update::versions.gotUpdate              = false;

    Update::currentState = UPDATE_STATE_IDLE;
}

void Update::processUpdateList(void)
{
    Update::versions.gotUpdate              = false;

    // check if the local update list exists
    int res = access(UPDATE_LOCALLIST, F_OK);

    if(res != 0) {                              // local update list doesn't exist, quit for now
        return;
    }

    Debug::out(LOG_DEBUG, "processUpdateList - starting");

    // open update list, parse Update::versions
    FILE *f = fopen(UPDATE_LOCALLIST, "rt");

    if(!f) {
        Debug::out(LOG_ERROR, "processUpdateList - couldn't open file %s", UPDATE_LOCALLIST);
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

        if(hwVersion == 1 && strncmp(what, "xilinx", 6) == 0) {                                     // HW version 1: use original xilinx version
            Update::versions.onServer.xilinx.fromString(ver);
            Update::versions.onServer.xilinx.setUrlAndChecksum(url, crc);
            continue;
        }

        if(hwVersion == 2 && hwHddIface == HDD_IF_ACSI && strncmp(what, "xlnx2a", 6) == 0) {        // HW ver 2 + ACSI IF: use xlnx2a
            Update::versions.onServer.xilinx.fromString(ver);
            Update::versions.onServer.xilinx.setUrlAndChecksum(url, crc);
            continue;
        } 

        if(hwVersion == 2 && hwHddIface == HDD_IF_SCSI && strncmp(what, "xlnx2s", 6) == 0) {        // HW ver 2 + SCSI IF: use xlnx2s
            Update::versions.onServer.xilinx.fromString(ver);
            Update::versions.onServer.xilinx.setUrlAndChecksum(url, crc);
            continue;
        }

        if(strncmp(what, "franz", 5) == 0) {
            Update::versions.onServer.franz.fromString(ver);
            Update::versions.onServer.franz.setUrlAndChecksum(url, crc);
            continue;
        }
    }

    fclose(f);

    //-------------------
    // now compare Update::versions - current with those on server, if anything new then set a flag
    if(Update::versions.current.app.isOlderThan( Update::versions.onServer.app )) {
        Update::versions.gotUpdate = true;
        Debug::out(LOG_DEBUG, "processUpdateList - APP is newer on server");
    }

    if(Update::versions.current.hans.isOlderThan( Update::versions.onServer.hans )) {
        Update::versions.gotUpdate = true;
        Debug::out(LOG_DEBUG, "processUpdateList - HANS is newer on server");
    }

    if(Update::versions.current.xilinx.isOlderThan( Update::versions.onServer.xilinx )) {
        Update::versions.gotUpdate = true;
        Debug::out(LOG_DEBUG, "processUpdateList - XILINX is newer on server");
    }

    if(Update::versions.current.franz.isOlderThan( Update::versions.onServer.franz )) {
        Update::versions.gotUpdate = true;
        Debug::out(LOG_DEBUG, "processUpdateList - FRANZ is newer on server");
    }

    Update::versions.updateListWasProcessed = true;         // mark that the update list was processed and don't need to do this again
    Debug::out(LOG_DEBUG, "processUpdateList - done");
}

void Update::deleteLocalUpdateComponents(void)
{
    unlink(UPDATE_LOCALLIST);
    unlink("/ce/update/hans.hex");
    unlink("/ce/update/franz.hex");
    unlink("/ce/update/xilinx.xsvf");       // xilinx - v.1
    unlink("/ce/update/xlnx2a.xsvf");       // xilinx - v.2 ACSI
    unlink("/ce/update/xlnx2s.xsvf");       // xilinx - v.2 SCSI
    unlink("/ce/update/app.zip");
}

void Update::downloadUpdateList(char *remoteUrl)
{
    // check for existence and possibly create update dir
  	int res = mkdir(UPDATE_LOCALPATH, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);		// mod: 0x775
	
	if(res == 0) {					// dir created
		Debug::out(LOG_DEBUG, "Update: directory %s was created.", UPDATE_LOCALPATH);
	} else {						// dir not created
		if(errno != EEXIST) {		// and it's not because it already exists...
			Debug::out(LOG_ERROR, "Update: failed to create settings directory - %s", strerror(errno));
		}
	}

    // remove old update list
    remove(UPDATE_LOCALLIST);

    // add request for download of the update list
    TDownloadRequest tdr;
    
    if(remoteUrl == NULL) {                         // no remote url specified? use default
        tdr.srcUrl = UPDATE_REMOTEURL;
    } else {                                        // got the remote url? use it
        tdr.srcUrl = remoteUrl;
    }
        
    tdr.dstDir          = UPDATE_LOCALPATH;
    tdr.downloadType    = DWNTYPE_UPDATE_LIST;
    tdr.checksum        = 0;                        // special case - don't check checsum
    tdr.pStatusByte     = NULL;                     // don't update this status byte
    Downloader::add(tdr);
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

    // new state - we're downloading the update
    Update::currentState = UPDATE_STATE_DOWNLOADING;
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
    if(!vLocal.isOlderThan(vServer)) {                      // if local is not older than on server, we're up to date, don't wait for this file
        return true;
    }

    if(vServer.downloadStatus != DWNSTATUS_DOWNLOAD_OK) {   // if it's not downloaded OK (yet), then false
        return false;
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
    if(!vLocal.isOlderThan(vServer)) {                      // not forcing update & local is not older than on server, don't download and quit
        vServer.downloadStatus = DWNSTATUS_DOWNLOAD_OK;     // when not downloading, pretend that we have it :)
        return;
    }

    // start the download
    TDownloadRequest tdr;
    tdr.srcUrl          = vServer.getUrl();
    tdr.checksum        = vServer.getChecksum();
    tdr.dstDir          = UPDATE_LOCALPATH;
    tdr.downloadType    = DWNTYPE_UPDATE_COMP;
    tdr.pStatusByte     = &vServer.downloadStatus;          // store download status here
    Downloader::add(tdr);
}

void Update::getLocalPathFromUrl(std::string url, std::string &localPath)
{
    std::string urlPath, fileName, finalFile;
    Utils::splitFilenameFromPath(url, urlPath, fileName);   // get just the filename from url
    
    finalFile = UPDATE_LOCALPATH;
    Utils::mergeHostPaths(finalFile, fileName);             // create final local filename with path

    localPath = finalFile;
}

int Update::state(void)
{
    if(currentState == UPDATE_STATE_IDLE) {                 // when idle, just return idle
        return UPDATE_STATE_IDLE;
    }

    if(currentState == UPDATE_STATE_DOWNLOADING) {          // when downloading, check if really still downloading
        Debug::out(LOG_DEBUG, "Update::state - downloading");
    
        if(allDownloadedOk()) {                             // if everything downloaded OK
            if(allNewComponentsDownloaded()) {              // check if the files are on disk
                Debug::out(LOG_DEBUG, "Update::state - downloaded and all components present");
                currentState = UPDATE_STATE_DOWNLOAD_OK;
            } else {                                        // some file is missing?
                Debug::out(LOG_DEBUG, "Update::state - downloaded but some component missing");
                currentState = UPDATE_STATE_DOWNLOAD_FAIL;
            }
        }
        
        if(someDownloadFailed()) {                          // if some download failed, fail
            Debug::out(LOG_DEBUG, "Update::state - some download failed");
            currentState = UPDATE_STATE_DOWNLOAD_FAIL;
        }
        
        return currentState;                                // return the current state (might be already updated)
    }

    // if we got here, the download was finished, return that state
    return currentState;
}

bool Update::allDownloadedOk(void)
{
    if( Update::versions.onServer.app.downloadStatus    == DWNSTATUS_DOWNLOAD_OK &&
        Update::versions.onServer.hans.downloadStatus   == DWNSTATUS_DOWNLOAD_OK &&
        Update::versions.onServer.xilinx.downloadStatus == DWNSTATUS_DOWNLOAD_OK &&
        Update::versions.onServer.franz.downloadStatus  == DWNSTATUS_DOWNLOAD_OK) {
        return true;
    }
    
    return false;
}


bool Update::someDownloadFailed(void)
{
    if( Update::versions.onServer.app.downloadStatus    == DWNSTATUS_DOWNLOAD_FAIL ||
        Update::versions.onServer.hans.downloadStatus   == DWNSTATUS_DOWNLOAD_FAIL ||
        Update::versions.onServer.xilinx.downloadStatus == DWNSTATUS_DOWNLOAD_FAIL ||
        Update::versions.onServer.franz.downloadStatus  == DWNSTATUS_DOWNLOAD_FAIL) {
        return true;
    }
    
    return false;
}

void Update::stateGoIdle(void)
{
    currentState = UPDATE_STATE_IDLE;
}

void Update::stateGoDownloadOK(void)
{
    currentState = UPDATE_STATE_DOWNLOAD_OK;
}

bool Update::createUpdateScript(void)
{
    FILE *f = fopen(UPDATE_SCRIPT, "wt");

    if(!f) {
        Debug::out(LOG_ERROR, "Update::createUpdateScript failed to create update script - %s", UPDATE_SCRIPT);
        return false;
    }

    if(Update::versions.current.app.isOlderThan( Update::versions.onServer.app )) {
        std::string appFileWithPath;
        Update::getLocalPathFromUrl(Update::versions.onServer.app.getUrl(), appFileWithPath);

        std::string appFilePath, appFileOnly;
        Utils::splitFilenameFromPath(appFileWithPath, appFilePath, appFileOnly);
        
        fprintf(f, "# updgrade of application\n");
        fprintf(f, "echo -e \"\\n----------------------------------\"\n");
        fprintf(f, "echo -e \">>> Updating Main App - START\"\n");
        fprintf(f, "rm -rf %s\n", (char *)              UPDATE_APP_PATH);	        // delete old app
        fprintf(f, "mkdir %s\n", (char *)              	UPDATE_APP_PATH);	        // create dir for new app
        fprintf(f, "cd %s\n", (char *)              	UPDATE_APP_PATH);	        // cd to that dir
		fprintf(f, "mv %s /ce/app\n",		            appFileWithPath.c_str());   // move the .zip file from update dir to the app dir
        fprintf(f, "unzip /ce/app/%s\n", (char *)       appFileOnly.c_str());	    // unzip it
        fprintf(f, "chmod 755 %s/cosmosex\n", (char *)  UPDATE_APP_PATH);	        // change permissions
		fprintf(f, "rm -f /ce/app/%s\n",				appFileOnly.c_str());       // delete the zip
        fprintf(f, "sync\n");                                                       // write caches to disk
        fprintf(f, "echo -e \"\\n>>> Updating Main App - END\"\n");
        fprintf(f, "\n\n");
    }

    if(Update::versions.current.xilinx.isOlderThan( Update::versions.onServer.xilinx )) {
        std::string fwFile;
        Update::getLocalPathFromUrl(Update::versions.onServer.xilinx.getUrl(), fwFile);

        const char *xlnxTag = getPropperXilinxTag();
        
        fprintf(f, "# updgrade of Xilinx FW\n");
        fprintf(f, "echo -e \"\\n----------------------------------\"\n");
        fprintf(f, "echo -e \">>> Updating Xilinx - START\"\n");
        fprintf(f, "/ce/update/flash_xilinx %s\n", (char *) fwFile.c_str());
        fprintf(f, "rm -f %s\n", (char *) fwFile.c_str());
		fprintf(f, "cat /ce/update/updatelist.csv | grep %s | sed -e 's/[^,]*,\\([^,]*\\).*/\\1/' > /ce/update/xilinx_current.txt \n", xlnxTag);
        fprintf(f, "echo -e \">>> Updating Xilinx - END\"\n");
        fprintf(f, "\n\n");
    }

    if(Update::versions.current.hans.isOlderThan( Update::versions.onServer.hans )) {
        std::string fwFile;
        Update::getLocalPathFromUrl(Update::versions.onServer.hans.getUrl(), fwFile);

        fprintf(f, "# updgrade of Hans FW\n");
        fprintf(f, "echo -e \"\\n----------------------------------\"\n");
        fprintf(f, "echo -e \">>> Updating Hans - START\"\n");
        fprintf(f, "/ce/update/flash_stm32 -x -w %s /dev/ttyAMA0\n", (char *) fwFile.c_str());
        fprintf(f, "rm -f %s\n", (char *) fwFile.c_str());
        fprintf(f, "echo -e \">>> Updating Hans - END\"\n");
        fprintf(f, "\n\n");
    }

    if(Update::versions.current.franz.isOlderThan( Update::versions.onServer.franz )) {
        std::string fwFile;
        Update::getLocalPathFromUrl(Update::versions.onServer.franz.getUrl(), fwFile);

        fprintf(f, "# updgrade of Franz FW\n");
        fprintf(f, "echo -e \"\\n----------------------------------\"\n");
        fprintf(f, "echo -e \">>> Updating Franz - START\"\n");
        fprintf(f, "/ce/update/flash_stm32 -y -w %s /dev/ttyAMA0\n", (char *) fwFile.c_str());
        fprintf(f, "rm -f %s\n", (char *) fwFile.c_str());
        fprintf(f, "echo -e \">>> Updating Franz - END\"\n");
        fprintf(f, "\n\n");
    }

    fclose(f);
    return true;
}

bool Update::createFlashFirstFwScript(void)
{
    FILE *f = fopen(UPDATE_SCRIPT, "wt");

    if(!f) {
        Debug::out(LOG_ERROR, "Update::createFlashFirstFwScript failed to create update script - %s", UPDATE_SCRIPT);
        return false;
    }

    const char *xlnxTag = getPropperXilinxTag();
        
    fprintf(f, "# flash first Xilinx FW \n");
    fprintf(f, "echo -e \"\\n----------------------------------\"\n");
    fprintf(f, "echo -e \">>> Writing first FW for Xilinx - START\"\n");
    fprintf(f, "/ce/update/flash_xilinx /ce/firstfw/%s.xsvf \n\n", xlnxTag);
    fprintf(f, "cp /ce/firstfw/xilinx_current.txt /ce/update/ \n\n");
    fprintf(f, "echo -e \">>> Writing first FW for Xilinx - END\"\n");
    
    fprintf(f, "# flash first Hans FW \n");
    fprintf(f, "echo -e \"\\n----------------------------------\"\n");
    fprintf(f, "echo -e \">>> Writing first FW for Hans - START\"\n");
    fprintf(f, "/ce/update/flash_stm32 -x -w /ce/firstfw/hans.hex /dev/ttyAMA0 \n\n");
    fprintf(f, "echo -e \">>> Writing first FW for Hans - END\"\n");

    fprintf(f, "# flash first Franz FW \n");
    fprintf(f, "echo -e \"\\n----------------------------------\"\n");
    fprintf(f, "echo -e \">>> Writing first FW for Franz - START\"\n");
    fprintf(f, "/ce/update/flash_stm32 -y -w /ce/firstfw/franz.hex /dev/ttyAMA0 \n\n");
    fprintf(f, "echo -e \">>> Writing first FW for Franz - END\"\n");

    fclose(f);
    return true;
}

bool Update::checkForUpdateListOnUsb(std::string &updateFilePath)
{
	DIR *dir = opendir((char *) "/mnt/");						    // try to open the dir
	
    updateFilePath.clear();
    
    if(dir == NULL) {                                 				// not found?
        Debug::out(LOG_DEBUG, "Update::checkForUpdateListOnUsb -- /mnt/ dir not found, fail");
        return false;
    }
    
    char path[512];
    memset(path, 0, 512);
    bool found = false;
    
    while(1) {                                                  	// while there are more files, store them
		struct dirent *de = readdir(dir);							// read the next directory entry
	
		if(de == NULL) {											// no more entries?
			break;
		}
	
		if(de->d_type != DT_DIR && de->d_type != DT_REG) {			// not a file, not a directory?
			continue;
		}

        if(strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
            continue;
        }
        
        // construct path
        strcpy(path, "/mnt/");
        strcat(path, de->d_name);
        strcat(path, "/");
        strcat(path, UPDATE_USBFILE);
        
        int res = access(path, F_OK);

        if(res != -1) {                                             // if it's not this error, then the file exists
            found = true;
            break;
        }
    }

	closedir(dir);	

    if(found) {
        Debug::out(LOG_DEBUG, "Update::checkForUpdateListOnUsb -- update found: %s", path);
        updateFilePath = path;
    } else {
        Debug::out(LOG_DEBUG, "Update::checkForUpdateListOnUsb -- update not found on usb");
    }
    
    return found;
}

const char *Update::getPropperXilinxTag(void)
{
    if(hwVersion == 1) {                    // v.1 ? it's xilinx
        return "xilinx";
    } else {                                // v.2 ?
        if(hwHddIface == HDD_IF_ACSI) {     // v.2 and ACSI? it's xlnx2a
            return "xlnx2a";
        } else {                            // v.2 and SCSI? it's xlnx2s
            return "xlnx2s";
        }
    }
    
    return "??wtf??";
}
