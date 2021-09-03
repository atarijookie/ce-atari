// vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab
#ifndef _MOUNTER_H_
#define _MOUNTER_H_

#include <string>
#include <queue>
#include <stdint.h>

#define MOUNTER_ACTION_MOUNT                0
#define MOUNTER_ACTION_UMOUNT               1
#define MOUNTER_ACTION_RESTARTNETWORK_ETH0  2
#define MOUNTER_ACTION_RESTARTNETWORK_WLAN0 3
#define MOUNTER_ACTION_SYNC                 4
#define MOUNTER_ACTION_MOUNT_ZIP            5

#define MOUNTACTION_STATE_NOT_STARTED   0
#define MOUNTACTION_STATE_IN_PROGRESS   1
#define MOUNTACTION_STATE_DONE          2

typedef struct {
    int     id;
    uint8_t    state;
    uint32_t   changeTime;
} TMountActionState;

typedef struct {
    int         action;
    bool        deviceNotShared;
    
    std::string devicePath;             // used when deviceNotShared is true
    
    struct {                            // used when deviceNotShared is false
        std::string host;
        std::string hostDir;
        bool        nfsNotSamba;
        
        std::string username;
        std::string password;
    } shared;

    std::string mountDir;               // location where it should be mounted
    
    int     mountActionStateId;         // TMountActionState.id, which should be updated on change in this mount action
} TMounterRequest;

extern "C" {
    void *mountThreadCode(void *ptr);
}

class Mounter 
{
public:
    bool mountShared(const char *host, const char *hostDir, bool nfsNotSamba, const char *mountDir, const char *username, const char *password);
    bool mountDevice(const char *devicePath, const char *mountDir);
    void mountZipFile(const char *zipFilePath, const char *mountDir);
    void umountIfMounted(const char *mountDir);
    void restartNetworkEth0(void);
    void restartNetworkWlan0(void);
    void sync(void);

    static int add(TMounterRequest &tmr);
    static int mas_getState(int id);
    void run(void);
    static void stop(void);

private:
    bool mount(const char *mountCmd, const char *mountDir);
    bool mount(const char*, const char*, const char*, const char*);

    bool isAlreadyMounted(const char *source);
    bool isMountdirUsed(const char *mountDir);
    bool tryUnmount(const char *mountDir);
    
    void createSource(const char *host, const char *hostDir, bool nfsNotSamba, char *source);
    
    bool mountDumpContains(const char *searchedString);
    bool wlan0IsPresent(void);
    bool checkIfUp( const char* ifname );
    bool getWpaSupplicantRunning(void);

    void copyTextFileToLog(const char *path);

    static pthread_mutex_t mountQueueMutex;
    static pthread_cond_t mountQueueNotEmpty;
    static std::queue<TMounterRequest> mountQueue;
    static volatile bool shouldStop;
};

#endif

