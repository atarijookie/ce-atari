#ifndef _MOUNTER_H_
#define _MOUNTER_H_

#include <string>

#define MOUNTER_ACTION_MOUNT			0
#define MOUNTER_ACTION_UMOUNT			1
#define MOUNTER_ACTION_RESTARTNETWORK	2
#define MOUNTER_ACTION_SYNC             3

typedef struct {
	int			action;
	bool		deviceNotShared;
	
	std::string	devicePath;				// used when deviceNotShared is true
	
	struct {							// used when deviceNotShared is false
		std::string host;
		std::string hostDir;
		bool		nfsNotSamba;
        
        std::string username;
        std::string password;
	} shared;

	std::string mountDir;				// location where it should be mounted
} TMounterRequest;

extern "C" {
	void mountAdd(TMounterRequest &tmr);
	void *mountThreadCode(void *ptr);
}

class Mounter 
{
public:
	bool mountShared(char *host, char *hostDir, bool nfsNotSamba, char *mountDir, char *username, char *password);
	bool mountDevice(char *devicePath, char *mountDir);
	void umountIfMounted(char *mountDir);
	void restartNetwork(void);
	void sync(void);

private:
	bool mount(char *mountCmd, char *mountDir);

	bool isAlreadyMounted(char *source);
	bool isMountdirUsed(char *mountDir);
	bool tryUnmount(char *mountDir);
	
	void createSource(char *host, char *hostDir, bool nfsNotSamba, char *source);
	
	bool mountDumpContains(char *searchedString);

    void copyTextFileToLog(char *path);
};

#endif

