#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>

#include "global.h"
#include "debug.h"
#include "settings.h"
#include "downloader.h"
#include "update.h"
#include "mounter.h"

#include "newscripts_zip.h"

Versions Update::versions;
int Update::currentState = UPDATE_STATE_IDLE;

extern THwConfig hwConfig;

volatile BYTE packageDownloadStatus = DWNSTATUS_WAITING;

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

        if(hwConfig.version == 1 && strncmp(what, "xilinx", 6) == 0) {                                      // HW version 1: use original xilinx version
            Update::versions.onServer.xilinx.fromString(ver);
            Update::versions.onServer.xilinx.setUrlAndChecksum(url, crc);
            continue;
        }

        if(hwConfig.version == 2 && hwConfig.hddIface == HDD_IF_ACSI && strncmp(what, "xlnx2a", 6) == 0) {  // HW ver 2 + ACSI IF: use xlnx2a
            Update::versions.onServer.xilinx.fromString(ver);
            Update::versions.onServer.xilinx.setUrlAndChecksum(url, crc);
            continue;
        } 

        if(hwConfig.version == 2 && hwConfig.hddIface == HDD_IF_SCSI && strncmp(what, "xlnx2s", 6) == 0) {  // HW ver 2 + SCSI IF: use xlnx2s
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
    
    unlink("/ce/update/xilinx.xsvf");       // xilinx - v.1
    unlink("/ce/update/xlnx2a.xsvf");       // xilinx - v.2 ACSI
    unlink("/ce/update/xlnx2s.xsvf");       // xilinx - v.2 SCSI

    system("rm -f /ce/update/*.hex /ce/update/*.zip");
    system("rm -f /tmp/*.hex /tmp/*.zip /tmp/*.xsvf");
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

    // delete local versions of components (which might be already downloaded) and start the download of newer components
    startPackageDownloadIfAnyComponentNewer();

    // new state - we're downloading the update
    Update::currentState = UPDATE_STATE_DOWNLOADING;
}

int Update::state(void)
{
    if(currentState == UPDATE_STATE_IDLE) {                 // when idle, just return idle
        return UPDATE_STATE_IDLE;
    }

    if(currentState == UPDATE_STATE_DOWNLOADING) {              // when downloading, check if really still downloading
        Debug::out(LOG_DEBUG, "Update::state - downloading");
    
        if(packageDownloadStatus == DWNSTATUS_DOWNLOAD_OK) {    // if everything downloaded OK
            Debug::out(LOG_DEBUG, "Update::state - package downloaded OK");
            currentState = UPDATE_STATE_DOWNLOAD_OK;
        }
        
        if(packageDownloadStatus == DWNSTATUS_DOWNLOAD_FAIL) { // if failed to download
            Debug::out(LOG_DEBUG, "Update::state - failed to download package");
            currentState = UPDATE_STATE_DOWNLOAD_FAIL;
        }
        
        return currentState;                                // return the current state (might be already updated)
    }

    // if we got here, the download was finished, return that state
    return currentState;
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
        fprintf(f, "/ce/update/update_app.sh\n");
    }

    if(Update::versions.current.xilinx.isOlderThan( Update::versions.onServer.xilinx )) {
        fprintf(f, "/ce/update/update_xilinx.sh\n");
    }

    if(Update::versions.current.hans.isOlderThan( Update::versions.onServer.hans )) {
        fprintf(f, "/ce/update/update_hans.sh\n");
    }

    if(Update::versions.current.franz.isOlderThan( Update::versions.onServer.franz )) {
        fprintf(f, "/ce/update/update_franz.sh\n");
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

    fprintf(f, "/ce/ce_firstfw.sh\n");                              // only thing needed is to run this first FW writing script
    fprintf(f, "rm -f %s\n", UPDATE_SCRIPT);                        // delete the update script, so we won't get stuck in the updating loop
    fprintf(f, "/ce/cesuper.sh &\n");                               // start the CE supervisor script, which will start CE app - it was killed at the beggining of ce_firstfw.sh
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
    if(hwConfig.version == 1) {                 // v.1 ? it's xilinx
        return "xilinx";
    } else {                                    // v.2 ?
        if(hwConfig.hddIface == HDD_IF_ACSI) {  // v.2 and ACSI? it's xlnx2a
            return "xlnx2a";
        } else {                                // v.2 and SCSI? it's xlnx2s
            return "xlnx2s";
        }
    }
    
    return "??wtf??";
}

void Update::createNewScripts(void)
{
    //------------
    // avoid running more than once by a simple bool
    static bool wasRunOnce = false;         // make sure this runs only once - not needed to run it more times

    if(wasRunOnce) {                        // if it was already runned, quit
        Debug::out(LOG_DEBUG, "Update::createNewScripts() - won't try to update scripts, already tried that once during this app run");
        return;
    }
    
    wasRunOnce = true;                      // mark that we've runned this once

    //------------
    // avoid running if not needed by a version check
    char appVerThis[12];
    Version::getAppVersion(appVerThis);                             // get version of this app (and these scripts)

    Settings s;
    char *scriptsVer = s.getString((char *) "SCRIPTS_VER", (char *) "XXXX-XX-XX");    // get version of scripts we have on disk

    if(strcmp(appVerThis, scriptsVer) == 0) {                       // app version matches the current scripts version? Don't update scripts.
        wasRunOnce = true;                      // mark that we've runned this once
        Debug::out(LOG_DEBUG, "Update::createNewScripts() - won't try to update scripts, because we already got the scripts in this version");
        return;
    }
    
    //------------
    // ok, it seems that we should update the scripts, so update them
    Debug::out(LOG_DEBUG, "Update::createNewScripts() - will try to update scripts");
    
    // write the data to file
    FILE *f = fopen("/tmp/newscripts.zip", "wb");
    
    if(!f) {
        Debug::out(LOG_ERROR, "Update::createNewScripts() - failed to open output file!");
        return;
    }

    fwrite(newscripts_zip, 1, newscripts_zip_len, f);
    fclose(f);
    
    system("sync");
    
    // run the script
    system("rm -rf /tmp/newscripts");                                       // remove dir if it exists
    system("mkdir -p /tmp/newscripts");                                     // create dir
    system("unzip -o /tmp/newscripts.zip -d /tmp/newscripts > /dev/null");  // extract script there
    system("chmod 755 /tmp/newscripts/copynewscripts.sh");                  // make the copying script executable
    system("/tmp/newscripts/copynewscripts.sh ");                           // execute the copying script
    
    //------------
    // last step: mark the version of the scripts we now have
    s.setString((char *) "SCRIPTS_VER", appVerThis);                                 // store version of scripts we now have on disk
    Debug::out(LOG_DEBUG, "Update::createNewScripts() - scripts updated to version %s", appVerThis);
}

void Update::startPackageDownloadIfAnyComponentNewer(void)
{
    unlink("/tmp/ce_update.zip");
    system("rm -f /tmp/*.zip /tmp/*.hex /tmp/*.xsvf" );

    bool a,b,c,d;
    a = Update::versions.current.app.isOlderThan   (Update::versions.onServer.app);
    b = Update::versions.current.hans.isOlderThan  (Update::versions.onServer.hans);
    c = Update::versions.current.xilinx.isOlderThan(Update::versions.onServer.xilinx);
    d = Update::versions.current.franz.isOlderThan (Update::versions.onServer.franz);
    
    if(a || b || c || d) {                                      // if some component is older than the newest on server, 
        TDownloadRequest tdr;
        tdr.srcUrl          = "http://joo.kie.sk/cosmosex/update/ce_update.zip";
        tdr.checksum        = 0;                                // special case - don't check checsum
        tdr.dstDir          = UPDATE_LOCALPATH;
        tdr.downloadType    = DWNTYPE_UPDATE_COMP;
        tdr.pStatusByte     = &packageDownloadStatus;           // store download status here
        Downloader::add(tdr);
    }
}

