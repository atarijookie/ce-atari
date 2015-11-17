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

#define DEV_CHECK_TIME_MS	        3000
#define UPDATE_CHECK_TIME           1000
#define INET_IFACE_CHECK_TIME       1000
#define UPDATE_SCRIPTS_TIME         10000

#define WEB_PARAMS_CHECK_TIME_MS    3000
#define WEB_PARAMS_FILE             "/tmp/ce_startupmode"

void readWebStartupMode(void);
bool inetIfaceReady(const char* ifrname);

void handleConfigStreams(ConfigStream *cs, int fd1, int fd2);
void updateUpdateState(void);
void checkInetIfEnabled(void);

void *periodicThreadCode(void *ptr)
{
	Debug::out(LOG_DEBUG, "Periodic thread starting...");

	DWORD nextDevFindTime       = Utils::getCurrentMs();                    // create a time when the devices should be checked - and that time is now
    DWORD nextUpdateCheckTime   = Utils::getEndTime(5000);                  // create a time when update download status should be checked
    DWORD nextWebParsCheckTime  = Utils::getEndTime(WEB_PARAMS_CHECK_TIME_MS);  // create a time when we should check for new params from the web page
    DWORD nextInetIfaceCheckTime= Utils::getEndTime(1000);  			    // create a time when we should check for inet interfaces that got ready
    
    Update::downloadUpdateList(NULL);                                       // download the list of components with the newest available versions
    
    ce_conf_createFifos();                                                  // if should run normally, create the ce_conf FIFOs
    
    DevFinder devFinder;

	while(sigintReceived == 0) {
        //------------------------------------
        // should we check if there are new params received from debug web page?
        if(Utils::getCurrentMs() >= nextWebParsCheckTime) {
            readWebStartupMode();
            nextWebParsCheckTime  = Utils::getEndTime(WEB_PARAMS_CHECK_TIME_MS);
        }

        //------------------------------------
        // should we check for inet interfaces that might came up?
        if(Utils::getCurrentMs() >= nextInetIfaceCheckTime) {
            checkInetIfEnabled();
            nextInetIfaceCheckTime  = Utils::getEndTime(INET_IFACE_CHECK_TIME);     // update the time when we should interfaces again
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

        //------------------------------------
        // should check the update status?
        if(Utils::getCurrentMs() >= nextUpdateCheckTime) {
            nextUpdateCheckTime   = Utils::getEndTime(UPDATE_CHECK_TIME);   // update the time when we should check update status again

            // should we process update list?
            if(!Update::versions.updateListWasProcessed) {              // if didn't process update list yet
                Update::processUpdateList();

                if(Update::versions.updateListWasProcessed) {           // if we processed the list, update config stream
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

void checkInetIfEnabled(void)
{
	//up/running state of inet interfaces
	static bool  state_eth0     = false;
	static bool  state_wlan0    = false;

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
    }
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

void readWebStartupMode(void)
{
    FILE *f = fopen(WEB_PARAMS_FILE, "rt");

    if(!f) {                                            // couldn't open file? nothing to do
        return;
    }

    char tmp[64];
    memset(tmp, 0, 64);

    fgets(tmp, 64, f);

    int ires, logLev;
    ires = sscanf(tmp, "ll%d", &logLev);            // read the param
    fclose(f);

    unlink(WEB_PARAMS_FILE);                        // remove the file so we won't read it again

    if(ires != 1) {                                 // failed to read the new log level? quit
        return;
    }

    if(logLev >= LOG_OFF && logLev <= LOG_DEBUG) {  // param is valid
        if(flags.logLevel == logLev) {              // but logLevel won't change?
            return;                                 // nothing to do
        }

        flags.logLevel = logLev;                    // set new log level
        Debug::out(LOG_INFO, "Log level changed from file %s to level %d", WEB_PARAMS_FILE, logLev);
    } else {                                        // on set wrong log level - switch to lowest log level
        flags.logLevel = LOG_ERROR;
        Debug::out(LOG_INFO, "Log level change invalid, switching to LOG_ERROR");
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

