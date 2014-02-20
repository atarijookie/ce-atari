#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include <signal.h>
#include <pthread.h>
#include <queue>          

#define LOGFILE1	"/tmp/mount.log"
#define LOGFILE2	"/tmp/mount.err"
#define MOUNTDUMP	"/tmp/mount.dmp"

#define MAX_STR_SIZE	1024

#include "mounter.h"
#include "utils.h"
#include "debug.h"

pthread_mutex_t mountThreadMutex = PTHREAD_MUTEX_INITIALIZER;
std::queue<TMounterRequest> mountQueue;

void mountAdd(TMounterRequest &tmr)
{
	pthread_mutex_lock(&mountThreadMutex);				// try to lock the mutex
	mountQueue.push(tmr);								// add this to queue
	pthread_mutex_unlock(&mountThreadMutex);			// unlock the mutex
}

void *mountThreadCode(void *ptr)
{
	Mounter mounter;
	
	Debug::out("Mount thread starting...");

	while(sigintReceived == 0) {
		pthread_mutex_lock(&mountThreadMutex);			// lock the mutex

		if(mountQueue.size() == 0) {					// nothing to do?
			pthread_mutex_unlock(&mountThreadMutex);	// unlock the mutex
			sleep(1);									// wait 1 second and try again
			continue;
		}
		
		TMounterRequest tmr = mountQueue.front();		// get the 'oldest' element from queue
		mountQueue.pop();								// and remove it form queue
		pthread_mutex_unlock(&mountThreadMutex);		// unlock the mutex

		if(tmr.action == MOUNTER_ACTION_MOUNT) {		// should we mount this?
			if(tmr.deviceNotShared) {					// mount device?
				mounter.mountDevice((char *) tmr.devicePath.c_str(), (char *) tmr.mountDir.c_str());	
			} else {									// mount shared?
				mounter.mountShared((char *) tmr.shared.host.c_str(), (char *) tmr.shared.hostDir.c_str(), tmr.shared.nfsNotSamba, (char *) tmr.mountDir.c_str());
			}
			
			continue;
		} 
		
		if(tmr.action == MOUNTER_ACTION_UMOUNT) {		// should umount?
			mounter.umountIfMounted((char *) tmr.mountDir.c_str());
			continue;
		}
		
		if(tmr.action == MOUNTER_ACTION_RESTARTNETWORK) {
			mounter.restartNetwork();
		}
	}
	
	Debug::out("Mount thread terminated.");
	return 0;
}

bool Mounter::mountDevice(char *devicePath, char *mountDir)
{
	char cmd[MAX_STR_SIZE];

	if(isAlreadyMounted(devicePath)) {
		Debug::out("Mounter::mountDevice -- The device %s is already mounted, not doing anything.\n", devicePath);
		return true;
	}

	snprintf(cmd, MAX_STR_SIZE, "sudo mount -v %s %s > %s 2> %s", devicePath, mountDir, LOGFILE1, LOGFILE2);

	int len = strnlen(cmd, MAX_STR_SIZE);	// get the length

	// if the final command string did not fit in the buffer
	if(len == MAX_STR_SIZE) {
		Debug::out("Mounter::mountDevice - cmd string not large enough for mount!\n");
		return false;
	}
	
	// now do the mount stuff
	return mount(cmd, mountDir);
}

bool Mounter::mountShared(char *host, char *hostDir, bool nfsNotSamba, char *mountDir)
{
	// build and run the command
	char cmd[MAX_STR_SIZE];
	char source[MAX_STR_SIZE];
	int len;
	
	createSource(host, hostDir, nfsNotSamba, source);		// create source path
	
	// check if we're not trying to mount something we already have mounted
	if(isAlreadyMounted(source)) {
		Debug::out("Mounter::mountShared -- The source %s is already mounted, not doing anything.\n", source);
		return true;
	}
		
	if(nfsNotSamba) {		// for NFS
		snprintf(cmd, MAX_STR_SIZE, "sudo mount -v -t nfs -o nolock %s %s > %s 2> %s", source, mountDir, LOGFILE1, LOGFILE2);
	} else {				// for Samba
		passwd *psw = getpwnam("pi");
		
		if(psw == NULL) {
			Debug::out("Mounter::mountShared - failed, because couldn't get uid and gid for user 'pi'\n");
			return false;
		}
		
		snprintf(cmd, MAX_STR_SIZE, "sudo mount -v -t cifs -o gid=%d,uid=%d,pass=%s %s %s > %s 2> %s", psw->pw_gid, psw->pw_uid, "password", source, mountDir, LOGFILE1, LOGFILE2);
	}

	len = strnlen(cmd, MAX_STR_SIZE);	// get the length

	// if the final command string did not fit in the buffer
	if(len == MAX_STR_SIZE) {
		Debug::out("Mounter::mountShared - cmd string not large enough for mount!\n");
		return false;
	}
	
	// now do the mount stuff
	return mount(cmd, mountDir);
}

bool Mounter::mount(char *mountCmd, char *mountDir)
{
	// create mount dir if possible
	int ires = mkdir(mountDir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);		// try to create the mount dir, mod: 0x775
	
	if(ires != 0 && errno != EEXIST) {										// if failed to create dir, and it's not because it already exists...
		Debug::out("Mounter::mount - failed to create directory %s", mountDir);
		return false;
	}

	// check if we should do unmount first
	if(isMountdirUsed(mountDir)) {
		if(!tryUnmount(mountDir)) {
			Debug::out("Mounter::mount - Mount dir %s is busy, and unmount failed, not doing mount!\n", mountDir);
			return false;
		}
		
		Debug::out("Mounter::mount - Mount dir %s was used and was unmounted.\n", mountDir);
	}
	
	// delete previous log files (if there are any)
	unlink(LOGFILE1);
	unlink(LOGFILE2);

	Debug::out("Mounter::mount - mount command:\n%s\n", mountCmd);
	
	// build and run the command
	int ret = system(mountCmd);
	
	// handle the result
	if(WIFEXITED(ret) && WEXITSTATUS(ret) == 0) {
		Debug::out("Mounter::mount - mount succeeded! (mount dir: %s)\n", mountDir);
		return true;
	} 
	
	Debug::out("Mounter::mount - mount failed. (mount dir: %s)\n", mountDir);
		
	// move the logs to mount dir
	char cmd[MAX_STR_SIZE];
		
	sprintf(cmd, "sudo mv %s %s/", LOGFILE1, mountDir);
	system(cmd);

	sprintf(cmd, "sudo mv %s %s/", LOGFILE2, mountDir);
	system(cmd);

	return false;
}

bool Mounter::isAlreadyMounted(char *source)
{
	return mountDumpContains(source);
}

bool Mounter::isMountdirUsed(char *mountDir)
{
	return mountDumpContains(mountDir);
}

bool Mounter::mountDumpContains(char *searchedString)
{
	char line[MAX_STR_SIZE];

	unlink(MOUNTDUMP);							// delete mount dump file
	
	// create new dump file and also filter it for searched string
	sprintf(line, "mount | grep '%s' > %s", searchedString, MOUNTDUMP);
	system(line);
	
	// open the file
	FILE *f = fopen(MOUNTDUMP, "rt");			// open the file
	
	if(!f) {									// if open failed, say that it's not mounted
		return false;
	}

	// search the file
	while(!feof(f)) {							// go through the lines of that file
		memset(line, 0, MAX_STR_SIZE);
		fgets(line, MAX_STR_SIZE, f);
		
		if(strstr(line, searchedString) != 0) {	// if the source was found in the mount dump, then it's already mounted
			fclose(f);
			return true;
		}
	}

	// on fail
	fclose(f);									// close mount dump file
	return false;								// if we got here, then we didn't find the mount
}

bool Mounter::tryUnmount(char *mountDir)
{
	char line[MAX_STR_SIZE];
	
	// build and execute the command
	snprintf(line, MAX_STR_SIZE, "sudo umount %s", mountDir);
	int ret = system(line);
	
	// handle the result
	if(WIFEXITED(ret) && WEXITSTATUS(ret) == 0) {
		Debug::out("Mounter::tryUnmount - umount succeeded\n");
		return true;
	} 
	
	return false;
}

void Mounter::createSource(char *host, char *hostDir, bool nfsNotSamba, char *source)
{
	if(strlen(host) < 1 || strlen(hostDir) < 1) {		// input string(s) too short? fail
		source[0] = 0;
		return;
	}

	if(nfsNotSamba) {									// for nfs
		int len;
		
		strcpy(source, host);
		len = strlen(source);
		
		if(source[len - 1] == '/') {					// ends with slash? remove it
			source[len - 1] = 0;
		}
		
		strcat(source, ":");							// append ':'
		
		if(hostDir[0] != '/') {							// doesn't start with slash? add it!
			strcat(source, "/");
		}
		
		strcat(source, hostDir);						// append the host dir
	} else {											// for samba
		strcpy(source, "//");							// start with double-slash
		
		strcat(source, host);							// append host
		
		if(hostDir[0] != '/') {							// host dir doesn't start with slash? add it
			strcat(source, "/");
		}
		
		strcat(source, hostDir);
	}
}

void Mounter::umountIfMounted(char *mountDir)
{
	if(isMountdirUsed(mountDir)) {				// if mountDir is used, umount
		tryUnmount(mountDir);
	}
}

void Mounter::restartNetwork(void)
{
	Debug::out("Mounter::restartNetwork - starting to restart the network\n");

	system("sudo ifdown eth0");
	system("sudo ifdown wlan0");

	system("sudo ifup eth0");
	system("sudo ifup wlan0");
	
	Debug::out("Mounter::restartNetwork - done\n");
}

