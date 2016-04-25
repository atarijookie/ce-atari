#ifndef _RETRYMODULE_H_
#define _RETRYMODULE_H_

/*
This is the RETRY MODULE.
    
PURPOSE: 
--------
When ACSI / SCSI command fails from Atari side point of view (transfer not finished, 
device doesn't respond), Atari should issue a RETRY command, which will try to transfer data once again,
but without affecting the state of disk / file / network modules...

HOW:
----
For READ operation:
  - after processing of original function from module, store the data and status
  - if data transfer fails, respond to RETRY command with stored data
  - don't let the original module handle it, because it might screw its internal state (e.g. file position)

For WRITE operations:
  - if the recvData() failed, we know the original call failed
  - make sure, that no state of any module should be changed before doing recvData()
  - alter the RETRY command in buffer to look like the original command, then let the original module to do its job

*/

class RetryModule 
{
public:
    RetryModule(void);
    ~RetryModule(void);
    
    bool gotThisCmd(BYTE *fullCmd, BYTE isIcd);
    
    void makeCmdCopy            (BYTE *fullCmd, BYTE  isIcd, BYTE  justCmd, BYTE  tag1, BYTE  tag2, BYTE  module);
    void restoreCmdFromCopy     (BYTE *fullCmd, BYTE &isIcd, BYTE &justCmd, BYTE &tag1, BYTE &tag2, BYTE &module);

    void copyDataAndStatus      (int dataDirection,  DWORD  count, BYTE *buffer, bool  statusWasSet, BYTE  status);
    void restoreDataAndStatus   (int &dataDirection, DWORD &count, BYTE *buffer, bool &statusWasSet, BYTE &status);
    
    int getDataDirection(void);
    
private:
    // from CCoreThread -- part of copied data which are available right after receiving command
    BYTE fullCmd[ACSI_CMD_SIZE];
    BYTE isIcd;
    BYTE justCmd;
    BYTE tag1, tag2;
    BYTE module;

    // from AcsiDataTrans -- part of copied data which are available after successfull READ operation
    int     dataDirection;
    DWORD   count;
    BYTE *  buffer;
    bool    statusWasSet;
    BYTE    status;
};

#endif
