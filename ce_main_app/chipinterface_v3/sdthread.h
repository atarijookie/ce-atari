#ifndef _SDTHREAD_H_
#define _SDTHREAD_H_

#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include "../utils.h"
#include "../debug.h"
#include "../global.h"
#include "../update.h"
#include "sectorfifo.h"

void *sdThreadCode(void *ptr);

void sdthread_getCardInfo(bool& isInit, bool& mediaChanged, int64_t& byteCapacity, int64_t& sectorCapacity);
void sdthread_startBackgroundTransfer(bool readNotWrite, int64_t sectorNo, DWORD count);

void sdthread_cancelCurrentOperation(void);

bool sdthread_sectorDataGet(BYTE *data);
bool sdthread_sectorDataPut(BYTE *data);

bool sdthread_waitUntilAllDataWritten(void);

#endif // _SDTHREAD_H_
