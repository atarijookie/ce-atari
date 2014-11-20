#ifndef _NETADAPTER_H_
#define _NETADAPTER_H_

#include <string>

#include "../acsidatatrans.h"
#include "../settings.h"
#include "../datatypes.h"
#include "../isettingsuser.h"

#include "sting.h"

#define MAX_HANDLE      32
#define BUFFER_SIZE     (1024 * 1024)

//-------------------------------------

class TNetConnection
{
public:
    TNetConnection() {          // contructor to init stuff
        initVars();
    }

    ~TNetConnection() {         // destructor to possibly close connection
        closeIt();
    }

    void closeIt(void) {
        if(fd != -1) {
            close(fd);
        }

        initVars();
    }

    void initVars(void) {
        fd          = -1;
        type        = 0;
        bytesToRead = 0;
        status      = TCLOSED;
    }

    int fd;                 // file descriptor of socket
    int type;               // TCP / UDP / ICMP

    int bytesToRead;        // how many bytes are waiting to be read
    int status;             // status of connection - open, closed, ...
};

//-------------------------------------

class NetAdapter: public ISettingsUser
{
public:
    NetAdapter(void);
    virtual ~NetAdapter();

    void reloadSettings(int type);
    void setAcsiDataTrans(AcsiDataTrans *dt);

    void processCommand(BYTE *command);
	
private:
    AcsiDataTrans   *dataTrans;
    BYTE            *cmd;

    BYTE            *dataBuffer;

    TNetConnection  cons[MAX_HANDLE];   // this holds the info to connections

    void loadSettings(void);

    void conOpen(void);                 // open connection
    void conClose(void);                // close connection            
    void conSend(void);                 // send data
    void conUpdateInfo(void);           // send connection info to ST
    void conReadData(void);             // receive data
    void conGetDataCount(void);         // get how many data there is
    void conLocateDelim(void);          // find string delimiter in received data

    void icmpSend(void);                // send ICMP packet
    void icmpGetDgrams(void);           // receive ICMP packets

    void resolveStart(void);            // resolve name to ip
    void resolveGetResp(void);          // retrieve the results of resolve
};

//-------------------------------------

#endif


