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
    
    bool gotThisCmd(uint8_t *fullCmd, uint8_t isIcd);
    
    void makeCmdCopy            (uint8_t *fullCmd, uint8_t  isIcd, uint8_t  justCmd, uint8_t  tag1, uint8_t  tag2, uint8_t  module);
    void restoreCmdFromCopy     (uint8_t *fullCmd, uint8_t &isIcd, uint8_t &justCmd, uint8_t &tag1, uint8_t &tag2, uint8_t &module);

    void copyDataAndStatus      (int dataDirection,  uint32_t  count, uint8_t *buffer, bool  statusWasSet, uint8_t  status);
    void restoreDataAndStatus   (int &dataDirection, uint32_t &count, uint8_t *buffer, bool &statusWasSet, uint8_t &status);
    
    int getDataDirection(void);
    
private:
    // from CCoreThread -- part of copied data which are available right after receiving command
    uint8_t fullCmd[ACSI_CMD_SIZE];
    uint8_t isIcd;
    uint8_t justCmd;
    uint8_t tag1, tag2;
    uint8_t module;

    // from AcsiDataTrans -- part of copied data which are available after successfull READ operation
    int     dataDirection;
    uint32_t   count;
    uint8_t *  buffer;
    bool    statusWasSet;
    uint8_t    status;
};

#endif
