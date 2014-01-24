#ifndef _MOUNTER_H_
#define _MOUNTER_H_

class Mounter 
{
public:
	bool mountShared(char *host, char *hostDir, bool nfsNotSamba, char *mountDir);
	bool mountDevice(char *devicePath, char *mountDir);

private:
	bool mount(char *mountCmd, char *mountDir);

	bool isAlreadyMounted(char *source);
	bool isMountdirUsed(char *mountDir);
	bool tryUnmount(char *mountDir);
	
	void createSource(char *host, char *hostDir, bool nfsNotSamba, char *source);
	
	bool mountDumpContains(char *searchedString);
};

#endif

