#include <string.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "../global.h"
#include "../debug.h"
#include "../utils.h"
#include "../libdospath/libdospath.h"
#include "extension.h"
#include "extensionhandler.h"
#include "extensiondefs.h"
#include "extensionrecvthread.h"

extern volatile sig_atomic_t sigintReceived;
extern Extension extensions[3];

void handleExtOpen(Extension* ext, uint8_t* data, uint32_t dataCount)
{
    ext->functionTable.clear();         // clear the function table

    uint8_t funcCount = data[0];                    // byte 0 - exported functions count
    uint8_t funcStoreCount = MIN(funcCount, MAX_EXPORTED_FUNCTIONS);

    ext->functionTable.storeReceivedSignatures(data + 1, funcStoreCount);       // signatures start at byte 1, store them
    ext->state = EXT_STATE_RUNNING;     // mark extension as running
}

void *extensionThreadCode(void *ptr)
{
    Debug::out(LOG_INFO, "Extension Socket thread starting...");

    int sock = createRecvSocket("EXT_SOCK_PATH");

    if(sock < 0) {                      // without socket this thread has no use
        return 0;
    }

    uint8_t bfr[EXT_BUFFER_SIZE];

    while(sigintReceived == 0) {
        struct timeval timeout;
        timeout.tv_sec = 1;                             // short timeout
        timeout.tv_usec = 0;

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);

        int res = select(sock + 1, &readfds, NULL, NULL, &timeout);     // wait for data or timeout here

        if(res < 0 || !FD_ISSET(sock, &readfds)) {          // if select() failed or cannot read from fd, skip rest
            continue;
        }

        if(bfr[0] != 0) {           // if buffer doesn't seem to be empty, clear it now
            memset(bfr, 0, sizeof(bfr));
        }

        ssize_t recvCnt = recv(sock, bfr, sizeof(bfr), 0);  // receive now

        if(recvCnt < 1) {                                   // nothing received?
            continue;
        }

        Debug::out(LOG_DEBUG, "extensionThreadCode: received size: %d", (int) recvCnt);

        uint8_t extensionId = bfr[0];                               // byte 0: extension id

        char functionName[32];
        memset(functionName, 0, sizeof(functionName));
        strncpy(functionName, (const char*) (bfr + 1), sizeof(functionName) - 1);   // bytes 1-32: function name

        uint8_t statusByte = bfr[33];                               // byte 33
        uint32_t dataCount = Utils::getDword(bfr + 34);             // bytes 34,35,36,37
        uint8_t* data = bfr + 38;                                   // data starts at index 38

        if(extensionId >= MAX_EXTENSIONS_OPEN) {        // bad extension id?
            Debug::out(LOG_WARNING, "extensionThreadCode - extension index %d out of bounds", extensionId);
            continue;
        }

        Extension* ext = &extensions[extensionId];  // use pointer to extension for simpler reading

        // function name to index
        int functionIndex = ext->functionTable.getFunctionIndexByName(functionName);

        if(functionIndex == -1) {                   // function index not found?
            Debug::out(LOG_WARNING, "extensionThreadCode - function index not found for function name: '%s'", functionName);
            continue;
        }

        if(functionIndex == FAKE_INDEX_OPEN) {      // on open
            ExtensionHandler::mutexLock();
            handleExtOpen(ext, data, dataCount);
            ExtensionHandler::mutexUnlock();
            continue;
        }

        if(functionIndex == FAKE_INDEX_CLOSE) {     // on close
            Debug::out(LOG_INFO, "extensionThreadCode - extension '%s' at index %d reported CLOSE, clearing internals", ext->name, extensionId);
            ExtensionHandler::mutexLock();
            ext->clear();
            ExtensionHandler::mutexUnlock();
            continue;
        }

        // it's not one of our special functions, so it's one of the functions from function table - let's handle it now
        ExtensionHandler::mutexLock();

        ext->response.store(statusByte, data, dataCount);    // store data

        // if the response is a path to file, we should do host-to-atari path conversion
        if(ext->functionTable.signatures[functionIndex].returnValueType == TYPE_PATH) {
            // TODO: convert ext->response.data to atari path
            // TODO: currently no method to to long-to-short path conversion???
        }

        ExtensionHandler::mutexUnlock();        // unlock mutex protecting extensions array

        ExtensionHandler::setSignal();          // notify possibly waiting command (see where ExtensionHandler::waitForSignal is called)
    }

    Debug::out(LOG_INFO, "Extension Socket thread terminated.");
    close(sock);
    return 0;
}
