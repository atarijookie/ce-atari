#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <unistd.h>

#include "screencastacsicommand.h"
#include "global.h"
#include "debug.h"
#include "utils.h"

#include "translated/gemdos_errno.h"

ScreencastAcsiCommand::ScreencastAcsiCommand(AcsiDataTrans *dt):dataTrans(dt)
{
    dataBuffer  = new uint8_t[SCREENCAST_BUFFER_SIZE];
    sharedMemoryOpen();
}

ScreencastAcsiCommand::~ScreencastAcsiCommand() 
{
    sharedMemoryClose();
	delete []dataBuffer;
}

void ScreencastAcsiCommand::sharedMemoryOpen(void)
{
	// initialize values
	sharedMemFd = -1;
	sharedMemPointer = NULL;
	sharedMemSemaphore = SEM_FAILED;

    // create semaphore
    std::string semaphoreName = Utils::dotEnvValue("SCREENCAST_SEMAPHORE_NAME");
    sharedMemSemaphore = sem_open(semaphoreName.c_str(), O_CREAT, 0666, 0);

    if(sharedMemSemaphore == SEM_FAILED) {
        Debug::out(LOG_ERROR, "ScreenCastAcsiCommand::openSharedMemory - failed to create semaphore: %d", errno);
    }

    // open the shared memory file
    std::string sharedMemoryName = Utils::dotEnvValue("SCREENCAST_MEMORY_NAME");
    sharedMemFd = shm_open(sharedMemoryName.c_str(), O_RDWR | O_CREAT, 0666);

    if(sharedMemFd < 0) {
        Debug::out(LOG_ERROR, "ScreenCastAcsiCommand::openSharedMemory - failed to create shared memory file: %d", errno);
        return;
    }

    // map the shared memory
    void* mmapRes = mmap(0, SCREENCAST_BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, sharedMemFd, 0);

    if(mmapRes != MAP_FAILED) {                         // on success, use as address
        ftruncate(sharedMemFd, SCREENCAST_BUFFER_SIZE);    // resize from 0 to required size
        sharedMemPointer = (uint8_t*) mmapRes;                // store pointer
        memset(sharedMemPointer, 0, SCREENCAST_BUFFER_SIZE);  // set all to zeros

        FILE *f = fopen("ce_logo.bin", "rb");           // if can open this logo file
        if(f) {
            fread(sharedMemPointer + 1, 1, 32032, f);   // fill the initial screencast buffer with logo
            fclose(f);
        } else {
            Debug::out(LOG_WARNING, "ScreenCastAcsiCommand::openSharedMemory - failed to load ce_logo.bin");
        }

        Debug::out(LOG_DEBUG, "ScreenCastAcsiCommand::openSharedMemory - mmap() success");
    } else {                        // on error, log error
        Debug::out(LOG_ERROR, "ScreenCastAcsiCommand::openSharedMemory - mmap() shared memory failed : %d", errno);
    }
}

void ScreencastAcsiCommand::sharedMemoryClose(void)
{
	if(sharedMemPointer) {                  // got pointer to shared memory?
	    munmap(sharedMemPointer, SCREENCAST_BUFFER_SIZE);
	    sharedMemPointer = NULL;
	}

    if(sharedMemFd >= 0) {                  // got fd for shared memory?
        close(sharedMemFd);
	    sharedMemFd = -1;

        std::string sharedMemoryName = Utils::dotEnvValue("SCREENCAST_MEMORY_NAME");
	    shm_unlink(sharedMemoryName.c_str());
	}

	if(sharedMemSemaphore != SEM_FAILED) {  // got semaphore?
	    sem_close(sharedMemSemaphore);
	    sharedMemSemaphore = SEM_FAILED;

        std::string semaphoreName = Utils::dotEnvValue("SCREENCAST_SEMAPHORE_NAME");
	    sem_unlink(semaphoreName.c_str());
	}
}

void ScreencastAcsiCommand::processCommand(uint8_t *command)
{
    cmd = command;

    if(dataTrans == 0) {
        Debug::out(LOG_ERROR, "ScreencastAcsiCommand::processCommand was called without valid dataTrans, can't tell ST that his went wrong...");
        return;
    }

    dataTrans->clear();                 // clean data transporter before handling

    Debug::out(LOG_DEBUG, "ScreencastAcsiCommand::processCommand");

    switch(cmd[4]) {
        case TRAN_CMD_SENDSCREENCAST:                    // ST sends screen buffer
            readScreen();
            break;
        case TRAN_CMD_SCREENCASTPALETTE:                    // ST sends screen buffer
            readPalette();
            break;
    }

    //>dataTrans->sendDataAndStatus();         // send all the stuff after handling, if we got any
    Debug::out(LOG_DEBUG, "ScreencastAcsiCommand done.");
}

void ScreencastAcsiCommand::readScreen(void)
{
    Debug::out(LOG_DEBUG, "ScreencastAcsiCommand::processCommand TRAN_CMD_SENDSCREENCAST");
    uint8_t iScreenmode = cmd[5];
    Debug::out(LOG_DEBUG, "ScreencastAcsiCommand::processCommand screenmode %d",iScreenmode);
    
    uint32_t byteCount = Utils::get24bits(cmd + 6);
    Debug::out(LOG_DEBUG, "ScreencastAcsiCommand::processCommand bytes %d",byteCount);
    
    if(iScreenmode<0 || iScreenmode>2) {                       // unknown screenmode?
        Debug::out(LOG_DEBUG, "ScreencastAcsiCommand::processCommand unknown screenmode %d",iScreenmode);
        dataTrans->setStatus(E_NOTHANDLED);
        return;
    }

    if(byteCount > (254 * 512)) {                                   // requesting to transfer more than 254 sectors at once? fail!
        Debug::out(LOG_DEBUG, "ScreencastAcsiCommand::processCommand too many sectors %d",byteCount);
        dataTrans->setStatus(EINTRN);
        return;
    }
    
    int mod = byteCount % 16;       // how many we got in the last 1/16th part?
    int pad = 0;
    
    if(mod != 0) {
        pad = 16 - mod;             // how many we need to add to make count % 16 equal to 0?
    }
    
    uint32_t transferSizeBytes = byteCount + pad;
    
    bool res;
    res = dataTrans->recvData(dataBuffer, transferSizeBytes);   // get data from Hans
    
    if(!res) {                                                  // failed to get data? internal error!
        dataTrans->setStatus(EINTRN);
        return;
    }

    if(sharedMemPointer) {                                // got shared memory? copy values there
        memcpy(sharedMemPointer + 33, dataBuffer, 32000); // 33 - 32032: screen data
    }

    dataTrans->setStatus(RW_ALL_TRANSFERED);                    // when all the data was written
}

void ScreencastAcsiCommand::readPalette(void)
{
    Debug::out(LOG_DEBUG, "ScreencastAcsiCommand::processCommand TRAN_CMD_SENDSCREENCAST");
    uint8_t iScreenmode = cmd[5];
    Debug::out(LOG_DEBUG, "ScreencastAcsiCommand::processCommand screenmode %d",iScreenmode);
    
    uint32_t byteCount = Utils::get24bits(cmd + 6);
    Debug::out(LOG_DEBUG, "ScreencastAcsiCommand::processCommand bytes %d",byteCount);
    
    if(iScreenmode<0 || iScreenmode>2) {                       // unknown screenmode?
        Debug::out(LOG_DEBUG, "ScreencastAcsiCommand::processCommand unknown screenmode %d",iScreenmode);
        dataTrans->setStatus(E_NOTHANDLED);
        return;
    }
    
    if(byteCount > (254 * 512)) {                                   // requesting to transfer more than 254 sectors at once? fail!
        Debug::out(LOG_DEBUG, "ScreencastAcsiCommand::processCommand too many sectors %d",byteCount);
        dataTrans->setStatus(EINTRN);
        return;
    }
    
    int mod = byteCount % 16;       // how many we got in the last 1/16th part?
    int pad = 0;
    
    if(mod != 0) {
        pad = 16 - mod;             // how many we need to add to make count % 16 equal to 0?
    }
    
    uint32_t transferSizeBytes = byteCount + pad;
    
    bool res;
    res = dataTrans->recvData(dataBuffer, transferSizeBytes);   // get data from Hans
    
    if(!res) {                                                  // failed to get data? internal error!
        dataTrans->setStatus(EINTRN);
        return;
    }

    // special case monochrome palette - always white + black
    if(iScreenmode == 2) {
        dataBuffer[0] = dataBuffer[1] = 0xff;
        dataBuffer[2] = dataBuffer[3] = 0x00;
    }

    if(sharedMemPointer) {                                // got shared memory? copy values there
        sharedMemPointer[0] = iScreenmode;                // 0: resolution
        memcpy(sharedMemPointer + 1, dataBuffer, 32);     // 1-32: palette
    }

    dataTrans->setStatus(RW_ALL_TRANSFERED);                    // when all the data was written
}
