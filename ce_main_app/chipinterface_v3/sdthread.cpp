#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <pthread.h>
#include <time.h>

#include "../utils.h"
#include "../debug.h"
#include "../global.h"
#include "../update.h"
#include "spisd.h"
#include "sdthread.h"

extern THwConfig hwConfig;

//------------------------------------
// ChipInterface v3 contains also object for accessing SD card as that is done via RPi SPI on v3.
// This object is created and destroyed elsewhere, so accesss it only when the pointer is not null.
extern SpiSD *spiSd;

//------------------------------------
pthread_mutex_t mtxSdRequest = PTHREAD_MUTEX_INITIALIZER;   // this mutex protects access to sdRWrequest and sectorFifo
pthread_cond_t  sdThreadCond = PTHREAD_COND_INITIALIZER;    // condition used to wake up SD thread on SD request

typedef struct {
    volatile bool    done;              // set to false when starting the request, set to true when request is done
    volatile bool    success;           // this holds success (true) of failure (false) when done=true

    volatile bool    readNotWrite;
    volatile int64_t sectorNo;
    volatile DWORD   count;
} SdRWrequest;

SdRWrequest sdRWrequest;
SectorFIFO  sectorFifo;

void sdthread_readWriteOperation(const SdRWrequest& sdRWrequestCopy);

void *sdThreadCode(void *ptr)
{
    DWORD nextCardCheck = Utils::getEndTime(1000);      // check in a while
    Debug::out(LOG_DEBUG, "sdThreadCode starting");

    // set read/write request to done, as we don't have any of them yet
    pthread_mutex_lock(&mtxSdRequest);
    sdRWrequest.done = true;
    pthread_mutex_unlock(&mtxSdRequest);

    while(sigintReceived == 0) {                        // while not terminated
        struct timespec max_wait = {0, 0};
        clock_gettime(CLOCK_REALTIME, &max_wait);
        max_wait.tv_sec += 1;                           // wake up in this interval without signal being set

        pthread_mutex_lock(&mtxSdRequest);              // lock the mutex
        pthread_cond_timedwait(&sdThreadCond, &mtxSdRequest, &max_wait);   // unlock, sleep; wake up, lock
        pthread_mutex_unlock(&mtxSdRequest);            // unlock the mutex

        if(spiSd == NULL) {                             // if the SD via SPI object doesn't exist, nothing to do here
            //Debug::out(LOG_DEBUG, "sdThreadCode - spiSd object is NULL");
            continue;                                   // restart loop, it will sleep 1 second on pthread_cond_timedwait()
        }

        if(Utils::getCurrentMs() >= nextCardCheck) {    // should check for card now?
            nextCardCheck = Utils::getEndTime(1000);    // check in a while

            if(spiSd->isInitialized()) {                 // when init, check if not removed
                //Debug::out(LOG_DEBUG, "sdThreadCode - checking if SD card still available");
                BYTE res = spiSd->mmcReadJustForTest(0);

                if(res != 0) {                          // when read failed, card probably removed
                    Debug::out(LOG_DEBUG, "sdThreadCode - SD card removed");
                    spiSd->clearStruct();
                }
            } else {                                    // when not init, try to init now
                //Debug::out(LOG_DEBUG, "sdThreadCode - trying to init SD card");
                spiSd->initCard();                       // try to init the card

                if(spiSd->isInitialized()) {             // if card initialized
                    Debug::out(LOG_DEBUG, "sdThreadCode - SD card found and initialized");
                }
            }
        }

        //-----------------------------------
        // do the R/W operation on SD if needed
        pthread_mutex_lock(&mtxSdRequest);
        SdRWrequest sdRWrequestCopy = sdRWrequest;  // make a copy of request struct, so we can immediatelly unlock mutex
        pthread_mutex_unlock(&mtxSdRequest);

        // do the R/W operation on SD if needed
        if(!sdRWrequestCopy.done) {                 // read/write not done yet?
            sdthread_readWriteOperation(sdRWrequestCopy);
        }
    }

    Debug::out(LOG_DEBUG, "sdThreadCode finished");
    return 0;
}

bool sdthread_readOperation(const SdRWrequest& sdRWrequestCopy)
{
    BYTE res;
    bool bRes;
    BYTE tmpBuffer[512];                    // copy data here, so we won't have to hold mutex locked long

    if(spiSd == NULL) {                     // don't have SPI SD object? quit
        return false;
    }

    if(sdRWrequestCopy.count == 1) {    // 1 sector
        res = spiSd->mmcRead(sdRWrequestCopy.sectorNo, tmpBuffer);     // read sector into temp buffer

        if(res == 0) {                  // on success
            bRes = sdthread_sectorDataPut(tmpBuffer);
            return bRes;
        }

        return false;                   // on fail

    } else {                            // 2+ sectors
        res = spiSd->mmcReadMultipleStart(sdRWrequestCopy.sectorNo);

        if(res != 0) {                  // start mutli fail?
            return false;
        }

        DWORD lastSectorIdx = sdRWrequestCopy.count - 1;        // do sectors 0 .. (count-1), so the (count -1) is the last index we'll do

        for(DWORD i=0; i<sdRWrequestCopy.count; i++) {
            bool isLastSector = (i == lastSectorIdx);
            res = spiSd->mmcReadMultipleOne(tmpBuffer, isLastSector);  // move one sector

            if(res != 0 || sigintReceived) {                    // if SD operation failed or SIGINT received
                return false;
            }

            bRes = sdthread_sectorDataPut(tmpBuffer);           // everything was fine, put new data into buffer

            if(!bRes) {                                         // failed to put data in FIFO?
                return false;
            }
        }

        return true;        // if loop finished and came here, success
    }

    // this should never happen, but if it does, fail
    return false;
}

bool sdthread_writeOperation(const SdRWrequest& sdRWrequestCopy)
{
    BYTE res;
    bool bRes;
    BYTE tmpBuffer[512];                // copy data here, so we won't have to hold mutex locked long

    if(spiSd == NULL) {                 // don't have SPI SD object? quit
        return false;
    }

    if(sdRWrequestCopy.count == 1) {                                // 1 sector
        bRes = sdthread_sectorDataGet(tmpBuffer);                   // get one sector of data into buffer

        if(!bRes) {                                                 // failed to get data from FIFO?
            return false;
        }

        res = spiSd->mmcWrite(sdRWrequestCopy.sectorNo, tmpBuffer); // write sector from temp buffer to card
        return (res == 0);                                          // good when res is 0

    } else {                            // 2+ sectors
        res = spiSd->mmcWriteMultipleStart(sdRWrequestCopy.sectorNo);

        if(res != 0) {                  // start mutli fail?
            return false;
        }

        DWORD lastSectorIdx = sdRWrequestCopy.count - 1;        // do sectors 0 .. (count-1), so the (count -1) is the last index we'll do

        for(DWORD i=0; i<sdRWrequestCopy.count; i++) {
            bRes = sdthread_sectorDataGet(tmpBuffer);           // get data into buffer

            if(!bRes) {                                         // failed to get data from FIFO?
                return false;
            }

            bool isLastSector = (i == lastSectorIdx);
            res = spiSd->mmcWriteMultipleOne(tmpBuffer, isLastSector); // move one sector

            if(res != 0 || sigintReceived) {                    // if SD operation failed or SIGINT received
                return false;
            }
        }

        // if loop finished and came here, success
        return true;
    }

    // this should never happen, but if it does, fail
    return false;
}

void sdthread_readWriteOperation(const SdRWrequest& sdRWrequestCopy)
{
    if(spiSd == NULL) {                     // don't have SPI SD object? quit
        return;
    }

    bool success;

    if(sdRWrequestCopy.readNotWrite) {      // read
        success = sdthread_readOperation(sdRWrequestCopy);
    } else {                                // write
        success = sdthread_writeOperation(sdRWrequestCopy);
    }

    // now that we've finished the R/W operation, we will mark it done and store success flag
    pthread_mutex_lock(&mtxSdRequest);
    sdRWrequest.done = true;
    sdRWrequest.success = success;
    pthread_mutex_unlock(&mtxSdRequest);
}

// The read / write operation is started by this single call. It will do the multiple sector
// transfer in the SD thread, data is moved between this and other thread using circular buffer.
void sdthread_startBackgroundTransfer(bool readNotWrite, int64_t sectorNo, DWORD count)
{
    pthread_mutex_lock(&mtxSdRequest);

    // clear fifo
    sectorFifo.clear();

    // store request params
    sdRWrequest.readNotWrite = readNotWrite;
    sdRWrequest.sectorNo = sectorNo;
    sdRWrequest.count = count;

    sdRWrequest.done = false;           // not done - we need to work on this one
    sdRWrequest.success = false;

    pthread_cond_signal(&sdThreadCond); // wake up SD thread
    pthread_mutex_unlock(&mtxSdRequest);
}

// Wait until sector data is read. If that fails (e.g. timeout), returns false.
bool sdthread_sectorDataGet(BYTE *data)
{
    DWORD timeoutTime = Utils::getEndTime(500);     // 500 ms timeout start
    bool success = false;

    // wait until there is something to read
    while(true) {
        pthread_mutex_lock(&mtxSdRequest);          // lock mutex

        if(sdRWrequest.done && !sdRWrequest.success) {  // if done, but with fail (possibly before whole operation finished)
            break;
        }

        if(Utils::getCurrentMs() >= timeoutTime || sigintReceived) {    // if timeout or SIGINT, fail
            break;
        }

        if(!sectorFifo.isEmpty()) {                 // not empty? can read!
            sectorFifo.read(data);                  // read whole sector from FIFO to this buffer
            success = true;
            break;
        }

        pthread_mutex_unlock(&mtxSdRequest);        // FIFO was empty, unlock mutex while we wait

        // FIFO empty, wait a while
        #ifndef ONPC
        bcm2835_delayMicroseconds(10);
        #else
        Utils::sleepMs(1);
        #endif
    }

    pthread_mutex_unlock(&mtxSdRequest);            // unlock mutex
    return success;                                 // return success or fail
}

// Put sector data in buffer and quit. If the writing of THIS sector failed
// in other thread, the NEXT sector write will return false.
bool sdthread_sectorDataPut(BYTE *data)
{
    DWORD timeoutTime = Utils::getEndTime(500);     // 500 ms timeout start
    bool success = false;

    // wait until FIFO not full, so we can put data in it
    while(true) {
        pthread_mutex_lock(&mtxSdRequest);          // lock mutex

        if(sdRWrequest.done && !sdRWrequest.success) {  // if done, but with fail (possibly before whole operation finished)
            break;
        }

        if(Utils::getCurrentMs() >= timeoutTime || sigintReceived) {    // if timeout or SIGINT, fail
            break;
        }

        if(!sectorFifo.isFull()) {                  // not full? can write!
            sectorFifo.write(data);                 // read write sector to FIFO from this buffer
            success = true;
            break;
        }

        pthread_mutex_unlock(&mtxSdRequest);        // FIFO was empty, unlock mutex while we wait

        // FIFO empty, wait a while
        #ifndef ONPC
        bcm2835_delayMicroseconds(10);
        #else
        Utils::sleepMs(1);
        #endif
    }

    pthread_mutex_unlock(&mtxSdRequest);            // unlock mutex
    return success;                                 // return success or fail
}

// Function waits until all data in write buffer has been written and returns success (true) or fail (false).
bool sdthread_waitUntilAllDataWritten(void)
{
    DWORD timeoutTime = Utils::getEndTime(500);     // 500 ms timeout start
    bool success = false;

    // wait until FIFO not empty, that would mean that all data was written
    while(true) {
        pthread_mutex_lock(&mtxSdRequest);          // lock mutex

        if(sdRWrequest.readNotWrite) {              // if this was READ operation, no write has to finish, so success and quit
            success = true;
            break;
        }

        if(sdRWrequest.done && !sdRWrequest.success) {  // if done, but with fail (possibly before whole operation finished)
            break;
        }

        if(Utils::getCurrentMs() >= timeoutTime || sigintReceived) {    // if timeout or SIGINT, fail
            break;
        }

        if(sectorFifo.isEmpty()) {                  // is empty? all written!
            success = true;
            break;
        }

        pthread_mutex_unlock(&mtxSdRequest);        // FIFO was empty, unlock mutex while we wait

        // FIFO empty, wait a while
        #ifndef ONPC
        bcm2835_delayMicroseconds(10);
        #else
        Utils::sleepMs(1);
        #endif
    }

    pthread_mutex_unlock(&mtxSdRequest);            // unlock mutex
    return success;                                 // return success or fail
}

// if we need to cancel current operation (e.g. transfer to ST failed, or SIGINT received, ...), call this function
void sdthread_cancelCurrentOperation(void)
{
    pthread_mutex_lock(&mtxSdRequest);

    // clear fifo
    sectorFifo.clear();

    // store request params
    sdRWrequest.done = true;            // we're done
    sdRWrequest.success = false;        // but failed

    pthread_mutex_unlock(&mtxSdRequest);
}

void sdthread_getCardInfo(bool& isInit, bool& mediaChanged, int64_t& byteCapacity, int64_t& sectorCapacity)
{
    // if we don't have SPI SD object
    if(spiSd == NULL) {
        isInit          = false;
        mediaChanged    = false;
        byteCapacity    = 0;
        sectorCapacity  = 0;

        return;
    }

    // if we have SPI SD object
    pthread_mutex_lock(&mtxSdRequest);              // lock mutex

    spiSd->getCardInfo(isInit, mediaChanged, byteCapacity, sectorCapacity); // get all info

    pthread_mutex_unlock(&mtxSdRequest);            // unlock mutex
}
