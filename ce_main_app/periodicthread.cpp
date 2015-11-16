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

void *periodicThreadCode(void *ptr)
{
	Debug::out(LOG_DEBUG, "Periodic thread starting...");

	DWORD nextDevFindTime       = Utils::getCurrentMs();                    // create a time when the devices should be checked - and that time is now
    DWORD nextUpdateCheckTime   = Utils::getEndTime(5000);                  // create a time when update download status should be checked
    DWORD nextWebParsCheckTime  = Utils::getEndTime(WEB_PARAMS_CHECK_TIME_MS);  // create a time when we should check for new params from the web page
    DWORD nextInetIfaceCheckTime= Utils::getEndTime(1000);  			    // create a time when we should check for inet interfaces that got ready
    
	//up/running state of inet interfaces
	bool  state_eth0 	= false;
	bool  state_wlan0 	= false;

    Update::downloadUpdateList(NULL);                                   // download the list of components with the newest available versions
    
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
		    nextInetIfaceCheckTime  = Utils::getEndTime(INET_IFACE_CHECK_TIME);     // update the time when we should interfaces again

            bool state_temp 		= 	inetIfaceReady("eth0"); 
			bool change_to_enabled	= 	(state_eth0!=state_temp)&&!state_eth0; 	    //was this iface disabled and current state changed to enabled?
			state_eth0 				= 	state_temp; 

            state_temp 				= 	inetIfaceReady("wlan0"); 
			change_to_enabled 		|= 	(state_wlan0!=state_temp)&&!state_wlan0;
			state_wlan0 			= 	state_temp; 
			
			if( change_to_enabled ){ 
				Debug::out(LOG_DEBUG, "Internet interface comes up: reload network mount settings");
				shared.translated->reloadSettings(SETTINGSUSER_TRANSLATED);
			}
		}

        //------------------------------------
        
        Utils::sleepMs(1000);
	}
	
	Debug::out(LOG_DEBUG, "Periodic thread terminated.");
	return 0;
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
