#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <net/if.h>

#include <signal.h>
#include <pthread.h>
#include <queue>          

#include "utils.h"
#include "debug.h"
#include "update.h"

#include "global.h"
#include "ccorethread.h"
#include "native/scsi.h"
#include "native/scsi_defs.h"
#include "gpio.h"
#include "mounter.h"
#include "downloader.h"
#include "update.h"
#include "config/netsettings.h"
#include "ce_conf_on_rpi.h" 

#include "devfinder.h"

#include "periodicthread.h"

extern THwConfig    hwConfig;
extern TFlags       flags;
extern DebugVars    dbgVars;

extern SharedObjects shared;

extern volatile BYTE updateListDownloadStatus;

#define DEV_CHECK_TIME_MS	        3000
#define UPDATE_CHECK_TIME           1000
#define INET_IFACE_CHECK_TIME       1000
#define UPDATE_SCRIPTS_TIME         10000

#define UPDATELIST_DOWNLOAD_TIME_ONFAIL     ( 1 * 60 * 1000)
#define UPDATELIST_DOWNLOAD_TIME_ONSUCCESS  (30 * 60 * 1000)

bool inetIfaceReady(const char* ifrname);

bool state_eth0     = false;
bool state_wlan0    = false;

void handleConfigStreams(ConfigStream *cs, int fd1, int fd2);
void updateUpdateState(void);
bool checkInetIfEnabled(void);

RPiConfig rpiConfig;
void getRaspberryPiInfo(void);
void readLineFromFile(const char *filename, char *buffer, int maxLen, const char *defValue);

void *periodicThreadCode(void *ptr)
{
	Debug::out(LOG_DEBUG, "Periodic thread starting...");

	DWORD nextDevFindTime               = Utils::getCurrentMs();            // create a time when the devices should be checked - and that time is now
    DWORD nextUpdateCheckTime           = Utils::getEndTime(5000);          // create a time when update download status should be checked
    DWORD nextInetIfaceCheckTime        = Utils::getEndTime(1000);          // create a time when we should check for inet interfaces that got ready
    
    DWORD nextUpdateListDownloadTime    = Utils::getEndTime(3000);          // try to download update list at this time
    
    ce_conf_createFifos();                                                  // if should run normally, create the ce_conf FIFOs
    getRaspberryPiInfo();
    
    DevFinder devFinder;

	while(sigintReceived == 0) {
        //------------------------------------
        // should we check for inet interfaces that might came up?
        if(Utils::getCurrentMs() >= nextInetIfaceCheckTime) {
            bool someIfChangedToEnabled = checkInetIfEnabled();
            nextInetIfaceCheckTime  = Utils::getEndTime(INET_IFACE_CHECK_TIME);     // update the time when we should interfaces again
            
            if(someIfChangedToEnabled) {                                            // if some IF has changed to enabled, try to download the update list again
                Debug::out(LOG_DEBUG, "periodicThreadCode -- eth0 or wlan0 changed to up, will now download update list");
                nextUpdateListDownloadTime = Utils::getEndTime(1000);
            }
		}

        //------------------------------------
        // should we do something related to devFinder?
        if(shared.devFinder_detachAndLook || shared.devFinder_look) {   // detach devices & look for them again, or just look for them?
            if(shared.devFinder_detachAndLook) {                    // if should also detach, do it
                pthread_mutex_lock(&shared.mtxScsi);
                pthread_mutex_lock(&shared.mtxTranslated);

                shared.translated->detachAllUsbMedia();             // detach all translated USB media
                shared.scsi->detachAllUsbMedia();                   // detach all RAW USB media

                pthread_mutex_unlock(&shared.mtxScsi);
                pthread_mutex_unlock(&shared.mtxTranslated);
            }
                
            // and now try to attach everything back
            devFinder.clearMap();						            // make all the devices appear as new
            devFinder.lookForDevChanges();					        // and now find all the devices

            shared.devFinder_detachAndLook  = false;
            shared.devFinder_look           = false;

            nextDevFindTime = Utils::getEndTime(DEV_CHECK_TIME_MS); // update the time when devices should be checked
        }
        
        //------------------------------------
        // should we check for the new devices?
		if(Utils::getCurrentMs() >= nextDevFindTime) {
			devFinder.lookForDevChanges();				            // look for devices attached / detached

			nextDevFindTime = Utils::getEndTime(DEV_CHECK_TIME_MS); // update the time when devices should be checked
		}

        if(Utils::getCurrentMs() >= nextUpdateListDownloadTime) {
            Debug::out(LOG_DEBUG, "periodicThreadCode -- will download update list now");
            Update::downloadUpdateList(NULL);                                                           // download the list of components with the newest available versions
            
            nextUpdateListDownloadTime = Utils::getEndTime(UPDATELIST_DOWNLOAD_TIME_ONSUCCESS);         // set the next download time in the future, so we won't do this immediately again
        }
        
        //------------------------------------
        // should check the update status?
        if(Utils::getCurrentMs() >= nextUpdateCheckTime) {
            nextUpdateCheckTime   = Utils::getEndTime(UPDATE_CHECK_TIME);                               // update the time when we should check update status again

            if(updateListDownloadStatus == DWNSTATUS_DOWNLOAD_FAIL) {                                   // download of update list fail?
                updateListDownloadStatus    = DWNSTATUS_WAITING;
                nextUpdateListDownloadTime  = Utils::getEndTime(UPDATELIST_DOWNLOAD_TIME_ONFAIL);       // set the next download time in the future, so we won't do this immediately again
                Debug::out(LOG_DEBUG, "periodicThreadCode -- update list download failed, will retry in %d seconds", UPDATELIST_DOWNLOAD_TIME_ONFAIL / 1000);
            }
            
            if(updateListDownloadStatus == DWNSTATUS_DOWNLOAD_OK) {                                     // download of update list good?
                updateListDownloadStatus    = DWNSTATUS_WAITING;
                nextUpdateListDownloadTime  = Utils::getEndTime(UPDATELIST_DOWNLOAD_TIME_ONSUCCESS);    // try to download the update list in longer time again
                Debug::out(LOG_DEBUG, "periodicThreadCode -- update list download good, will retry in %d minutes", UPDATELIST_DOWNLOAD_TIME_ONSUCCESS / (60 * 1000));
            
                if(!Update::versions.updateListWasProcessed) {                                          // Didn't process update list yet? process it
                    Update::processUpdateList();
                }

                if(Update::versions.updateListWasProcessed) {                                           // if we processed the list, update config stream
                    // if the config screen is shown, then update info on it
                    pthread_mutex_lock(&shared.mtxConfigStreams);

                    shared.configStream.acsi->fillUpdateWithCurrentVersions();
                    shared.configStream.web->fillUpdateWithCurrentVersions();
                    shared.configStream.term->fillUpdateWithCurrentVersions();

                    pthread_mutex_unlock(&shared.mtxConfigStreams);
                }
            }

            updateUpdateState();
        }

        //------------------------------------
        // config streams handling
        
        handleConfigStreams(shared.configStream.web,    shared.configPipes.web.fd1,     shared.configPipes.web.fd2);
        handleConfigStreams(shared.configStream.term,   shared.configPipes.term.fd1,    shared.configPipes.term.fd2);
        //------------------------------------
        
        Utils::sleepMs(50);
	}
	
	Debug::out(LOG_DEBUG, "Periodic thread terminated.");
	return 0;
}

bool checkInetIfEnabled(void)
{
	//up/running state of inet interfaces
    bool state_temp 		= 	inetIfaceReady("eth0"); 
    bool change_to_enabled	= 	(state_eth0!=state_temp)&&!state_eth0; 	    //was this iface disabled and current state changed to enabled?
    state_eth0 				= 	state_temp; 

    state_temp 				= 	inetIfaceReady("wlan0"); 
    change_to_enabled 		|= 	(state_wlan0!=state_temp)&&!state_wlan0;
    state_wlan0 			= 	state_temp; 
    
    if( change_to_enabled ){ 
        Debug::out(LOG_DEBUG, "Internet interface comes up: reload network mount settings");
        
        pthread_mutex_lock(&shared.mtxTranslated);
        shared.translated->reloadSettings(SETTINGSUSER_TRANSLATED);
        pthread_mutex_unlock(&shared.mtxTranslated);
        
        return true;            // some IF changed to enabled
    }
    
    return false;               // no IF changed to enabled
}

void updateUpdateState(void)
{
    int updateState = Update::state();                              // get the update state
    switch(updateState) {
        case UPDATE_STATE_DOWNLOADING:
        pthread_mutex_lock(&shared.mtxConfigStreams);
        
        // refresh config screen with download status
        shared.configStream.acsi->fillUpdateDownloadWithProgress();
        shared.configStream.web->fillUpdateDownloadWithProgress();
        shared.configStream.term->fillUpdateDownloadWithProgress();

        pthread_mutex_unlock(&shared.mtxConfigStreams);
        break;

        //-----------
        case UPDATE_STATE_DOWNLOAD_FAIL:
        pthread_mutex_lock(&shared.mtxConfigStreams);
        
        // show fail message on config screen
        shared.configStream.acsi->showUpdateDownloadFail();
        shared.configStream.web->showUpdateDownloadFail();
        shared.configStream.term->showUpdateDownloadFail();

        pthread_mutex_unlock(&shared.mtxConfigStreams);

        Update::stateGoIdle();
        Debug::out(LOG_ERROR, "Update state - download failed");
        break;

        //-----------
        case UPDATE_STATE_DOWNLOAD_OK:

        pthread_mutex_lock(&shared.mtxConfigStreams);

        // check if any of the config streams shows download page
        bool shownA = shared.configStream.acsi->isUpdateDownloadPageShown();
        bool shownW = shared.configStream.web->isUpdateDownloadPageShown();
        bool shownT = shared.configStream.term->isUpdateDownloadPageShown();
        
        pthread_mutex_unlock(&shared.mtxConfigStreams);
        
        if(!shownA && !shownW && !shownT) {         // if user is NOT waiting on download page (cancel pressed), don't update
            Update::stateGoIdle();
            Debug::out(LOG_DEBUG, "Update state - download OK, but user is not on download page - NOT doing update");
        } else {                                    // if user is waiting on download page, aplly update
            bool res = Update::createUpdateScript();

            if(!res) {
                pthread_mutex_lock(&shared.mtxConfigStreams);

                shared.configStream.acsi->showUpdateError();
                shared.configStream.web->showUpdateError();
                shared.configStream.term->showUpdateError();
                
                pthread_mutex_unlock(&shared.mtxConfigStreams);

                Debug::out(LOG_ERROR, "Update state - download OK, failed to create update script - NOT doing update");
            } else {
                Debug::out(LOG_INFO, "Update state - download OK, update script created, will do update.");
                sigintReceived = 1;
            }
        }

        break;
    }
}            

void handleConfigStreams(ConfigStream *cs, int fd1, int fd2)
{
    if(fd1 <= 0 || fd2 <= 0) {                              // missing handle? fail
        return;
    }

    int bytesAvailable;
    int ires = ioctl(fd1, FIONREAD, &bytesAvailable);       // how many bytes we can read?

    if(ires != -1 && bytesAvailable >= 3) {                 // if there are at least 3 bytes waiting
        BYTE cmd[6] = {0, 'C', 'E', 0, 0, 0};
        ires = read(fd1, cmd + 3, 3);                       // read the byte triplet
        
        Debug::out(LOG_DEBUG, "confStream - through FIFO: %02x %02x %02x (ires = %d)", cmd[3], cmd[4], cmd[5], ires);
        
        pthread_mutex_lock(&shared.mtxConfigStreams);
        cs->processCommand(cmd, fd2);
        pthread_mutex_unlock(&shared.mtxConfigStreams);
    }
}

// Is a particular internet interface up, running and has an IP address?
bool inetIfaceReady(const char* ifrname)
{
	struct ifreq ifr;
	bool up_and_running;

	//temporary socket
	int socketfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (socketfd == -1){
        return false;
	}
			
	memset( &ifr, 0, sizeof(ifr) );
	strcpy( ifr.ifr_name, ifrname );
	
	if( ioctl( socketfd, SIOCGIFFLAGS, &ifr ) != -1 ) {
	    up_and_running = (ifr.ifr_flags & ( IFF_UP | IFF_RUNNING )) == ( IFF_UP | IFF_RUNNING );
	    
		//it's only ready and usable if it has an IP address
        if( up_and_running && ioctl(socketfd, SIOCGIFADDR, &ifr) != -1 ){
	        if( ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr==0 ){
			    up_and_running = false;
			} 
		} else {
		    up_and_running = false;
		}
	    //Debug::out(LOG_DEBUG, "inetIfaceReady ip: %s %s", ifrname, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
	} else {
	    up_and_running = false;
	}

	int result=0;
	do {
        result = close(socketfd);
    } while (result == -1 && errno == EINTR);	

	return up_and_running; 
}

void getRaspberryPiInfo(void)
{
    // first parse the files, so we won't have to do this in C 
    system("cat /proc/cpuinfo | grep 'Serial' | tr -d ' ' | awk -F ':' '{print $2}' > /tmp/rpiserial.txt");
    system("cat /proc/cpuinfo | grep 'Revision' | tr -d ' ' | awk -F ':' '{print $2}' > /tmp/rpirevision.txt");
    system("dmesg | grep 'Machine model' | awk -F ': ' '{print $2}' > /tmp/rpimodel.txt");
    
    // read in the data
    readLineFromFile("/tmp/rpiserial.txt",      rpiConfig.serial,   20, "unknown");
    readLineFromFile("/tmp/rpirevision.txt",    rpiConfig.revision,  8, "unknown");
    readLineFromFile("/tmp/rpimodel.txt",       rpiConfig.model,    40, "Raspberry Pi unknown model");
    
    // print to log file in debug mode
    Debug::out(LOG_DEBUG, "RPi serial  : %s", rpiConfig.serial);
    Debug::out(LOG_DEBUG, "RPi revision: %s", rpiConfig.revision);
    Debug::out(LOG_DEBUG, "RPi model   : %s", rpiConfig.model);
}

void readLineFromFile(const char *filename, char *buffer, int maxLen, const char *defValue)
{
    memset(buffer, 0, maxLen);                  // clear the buffer where the value should be stored
    
    FILE *f = fopen(filename, "rt");            // open the file
    
    if(!f) {                                    // if failed to open, copy in the default value
        strncpy(buffer, defValue, maxLen - 1);
        return;
    }

    char *res = fgets(buffer, maxLen - 1, f);   // try to read the line
    
    if(res == NULL) {
        strncpy(buffer, defValue, maxLen - 1);  // failed to read the line? use default value
        return;
    }

    fclose(f);                                  // close the file
}
