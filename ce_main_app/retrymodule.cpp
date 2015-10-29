#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "global.h"
#include "debug.h"
#include "utils.h"
#include "gpio.h"
#include "acsidatatrans.h"
#include "native/scsi_defs.h"
#include "retrymodule.h"

RetryModule::RetryModule(void)
{
    memset(fullCmd, 0, ACSI_CMD_SIZE);
    buffer = new BYTE[ACSI_BUFFER_SIZE];
    
    dataDirection   = DATA_DIRECTION_UNKNOWN;
    count           = 0;
    statusWasSet    = false;
    status          = 0;    
}

RetryModule::~RetryModule(void)
{
    delete []buffer;
}

void RetryModule::makeCmdCopy(BYTE *fullCmd, BYTE isIcd, BYTE justCmd, BYTE tag1, BYTE tag2, BYTE module)
{
    if(module == HOSTMOD_RETRY) {       // if it's this module, don't make copy of anything
        return;
    }
    
    // make copy of every input param
    memcpy(this->fullCmd, fullCmd, ACSI_CMD_SIZE);
    this->isIcd   = isIcd;
    this->justCmd = justCmd;
    this->tag1    = tag1;
    this->tag2    = tag2;
    this->module  = module;
}

void RetryModule::restoreCmdFromCopy(BYTE *fullCmd, BYTE &isIcd, BYTE &justCmd, BYTE &tag1, BYTE &tag2, BYTE &module)
{
    // restore the input params using stored values
    memcpy(fullCmd, this->fullCmd, ACSI_CMD_SIZE);
    isIcd      = this->isIcd;
    justCmd    = this->justCmd;
    tag1       = this->tag1;
    tag2       = this->tag2;
    module     = this->module;
}

void RetryModule::copyDataAndStatus(int dataDirection, DWORD count, BYTE *buffer, bool statusWasSet, BYTE status)
{
    this->dataDirection   = dataDirection;
    this->count           = count;
    this->statusWasSet    = statusWasSet;
    this->status          = status;

    if(dataDirection == DATA_DIRECTION_READ) {      // if it's READ operation, copy also data
        DWORD copySize = (count < ACSI_BUFFER_SIZE) ? count : ACSI_BUFFER_SIZE;
        memcpy(this->buffer, buffer, copySize);
    }
}

void RetryModule::restoreDataAndStatus(int &dataDirection, DWORD &count, BYTE *buffer, bool &statusWasSet, BYTE &status)
{
    dataDirection   = this->dataDirection;
    count           = this->count;
    statusWasSet    = this->statusWasSet;
    status          = this->status;

    if(dataDirection == DATA_DIRECTION_READ) {      // if it's READ operation, copy also data
        DWORD copySize = (count < ACSI_BUFFER_SIZE) ? count : ACSI_BUFFER_SIZE;
        memcpy(buffer, this->buffer, copySize);
    }
}

int RetryModule::getDataDirection(void)
{
    return dataDirection;
}

