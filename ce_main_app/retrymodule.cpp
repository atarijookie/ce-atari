#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "global.h"
#include "debug.h"
#include "utils.h"
#include "acsidatatrans.h"
#include "native/scsi_defs.h"
#include "retrymodule.h"

RetryModule::RetryModule(void)
{
    memset(fullCmd, 0, ACSI_CMD_SIZE);
    buffer = new uint8_t[ACSI_BUFFER_SIZE];
    
    dataDirection   = DATA_DIRECTION_UNKNOWN;
    count           = 0;
    statusWasSet    = false;
    status          = 0;    
}

RetryModule::~RetryModule(void)
{
    delete []buffer;
}

bool RetryModule::gotThisCmd(uint8_t *fullCmd, uint8_t isIcd) 
{
    int cmdMismatch, commandLength;
    
    if(isIcd) {             // for ICD commands - compare the first 11 bytes 
        commandLength = 11;
    } else {                // for non-ICD commads - compare the first 6 bytes
        commandLength = 6;
    }
    
    cmdMismatch = memcmp(this->fullCmd, fullCmd, commandLength);    // do the compression of previous and current command
    
    if(cmdMismatch == 0) {      // the commands match? we got this
        return true;
    }
    
    return false;               // commands mismatch, we don't have this
}

void RetryModule::makeCmdCopy(uint8_t *fullCmd, uint8_t isIcd, uint8_t justCmd, uint8_t tag1, uint8_t tag2, uint8_t module)
{
    // make copy of every input param
    memcpy(this->fullCmd, fullCmd, ACSI_CMD_SIZE);
    this->isIcd   = isIcd;
    this->justCmd = justCmd;
    this->tag1    = tag1;
    this->tag2    = tag2;
    this->module  = module;
}

void RetryModule::restoreCmdFromCopy(uint8_t *fullCmd, uint8_t &isIcd, uint8_t &justCmd, uint8_t &tag1, uint8_t &tag2, uint8_t &module)
{
    // restore the input params using stored values
    memcpy(fullCmd, this->fullCmd, ACSI_CMD_SIZE);
    isIcd      = this->isIcd;
    justCmd    = this->justCmd;
    tag1       = this->tag1;
    tag2       = this->tag2;
    module     = this->module;
}

void RetryModule::copyDataAndStatus(int dataDirection, uint32_t count, uint8_t *buffer, bool statusWasSet, uint8_t status)
{
    this->dataDirection   = dataDirection;
    this->count           = count;
    this->statusWasSet    = statusWasSet;
    this->status          = status;

    if(dataDirection == DATA_DIRECTION_READ) {      // if it's READ operation, copy also data
        uint32_t copySize = (count < ACSI_BUFFER_SIZE) ? count : ACSI_BUFFER_SIZE;
        memcpy(this->buffer, buffer, copySize);
    }
}

void RetryModule::restoreDataAndStatus(int &dataDirection, uint32_t &count, uint8_t *buffer, bool &statusWasSet, uint8_t &status)
{
    dataDirection   = this->dataDirection;
    count           = this->count;
    statusWasSet    = this->statusWasSet;
    status          = this->status;

    if(dataDirection == DATA_DIRECTION_READ) {      // if it's READ operation, copy also data
        uint32_t copySize = (count < ACSI_BUFFER_SIZE) ? count : ACSI_BUFFER_SIZE;
        memcpy(buffer, this->buffer, copySize);
    }
}

int RetryModule::getDataDirection(void)
{
    return dataDirection;
}

