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
#include "extensionhandler.h"
#include "extensiondefs.h"
#include "extension.h"

pthread_mutex_t extensionMutex = PTHREAD_MUTEX_INITIALIZER;
Extension extensions[3];

void ExtensionHandler::mutexLock(const char* file, int line)
{
    // Debug::out(LOG_DEBUG, "mutexLock() - %s : %d", file, line);
    pthread_mutex_lock(&extensionMutex);
}

void ExtensionHandler::mutexUnlock(const char* file, int line)
{
    // Debug::out(LOG_DEBUG, "mutexUnlock() - %s : %d", file, line);
    pthread_mutex_unlock(&extensionMutex);
}

// Note: it would be nicer to do waitForSignal using pthread_cond_timedwait, but that was getting stuck, 
// so this is the simplest working alternative.
volatile sig_atomic_t extSignalReceived = 0;

void ExtensionHandler::waitForSignal(uint32_t ms)
{
    uint32_t start = Utils::getCurrentMs();

    while(1) {
        uint32_t now = Utils::getCurrentMs();
        uint32_t diff = now - start;

        if(extSignalReceived) {     // got signal? quit now
            Debug::out(LOG_DEBUG, "waitForSignal - signal received after %d ms", diff);
            break;
        }

        if(diff >= ms) {            // timeout? quit
            Debug::out(LOG_DEBUG, "waitForSignal timeout after %d ms", diff);
            break;
        }

        Utils::sleepMs(1);
    }
}

void ExtensionHandler::setSignal(void)
{
    extSignalReceived = 1;
}

void ExtensionHandler::clearSignal(void)
{
    extSignalReceived = 0;
}

ExtensionHandler::ExtensionHandler(void)
{
    pthread_mutex_init(&extensionMutex, NULL);

    dataTrans = 0;
    byteCountInDataBuffer = 0;

    dataBuffer  = new uint8_t[EXT_BUFFER_SIZE];
    dataBuffer2 = new uint8_t[EXT_BUFFER_SIZE];
}

ExtensionHandler::~ExtensionHandler()
{
    delete []dataBuffer;
    delete []dataBuffer2;
}

void ExtensionHandler::setAcsiDataTrans(AcsiDataTrans *dt)
{
    dataTrans = dt;
}

void ExtensionHandler::processCommand(uint8_t *command)
{
    AcsiCommand *cmd = (AcsiCommand*) command;

    uint8_t justCmd = cmd->idAndCmd & 0x1f;        // get just the command (remove ACSI ID)
    cmd4 = cmd->arg1;
    cmd5 = cmd->arg2;

    if(dataTrans == 0) {
        Debug::out(LOG_ERROR, "ExtensionHandler::processCommand was called without valid dataTrans, can't tell ST that his went wrong...");
        return;
    }

    dataTrans->clear();         // clean data transporter before handling

    Debug::out(LOG_DEBUG, "ExtensionHandler::processCommand -- CMD: %02x %02x %02x %02x %02x %02x - %s", 
                command[0], command[1], command[2], command[3], command[4], command[5],
                getCommandName(justCmd));

    uint32_t expectedBytesOnRead = 0;   // this will hold count of bytes we want to return on read (will stay 0 for write)

    // if we're handling WRITE commands, we can get the data
    if(justCmd == CMD_CALL_RAW_WRITE || justCmd == CMD_CALL_LONG_WRITE_ARGS) {
        byteCountInDataBuffer = ((uint32_t) cmd->sectorCount) * 512;  // sectors to bytes - this much data in recv buffer

        if(byteCountInDataBuffer > 0) {
            Debug::out(LOG_DEBUG, "justCmd: %02x, will receive %d bytes first", justCmd, byteCountInDataBuffer);

            // prepare the data buffer header in a form that can be sent directly to extension without further modification
            dataBuffer[0] = 'D';        // this buffer has data and DA is the marker
            dataBuffer[1] = 'A';
            Utils::storeDword(dataBuffer + 2, byteCountInDataBuffer);   // bytes 2,3,4,5
            dataTrans->recvData(dataBuffer + 6, byteCountInDataBuffer); // bytes 6 and next
            byteCountInDataBuffer += 6;                 // we got 6 more bytes in this buffer
        }
    } else {                                        // not write, so clear 1 kB from start
        byteCountInDataBuffer = 0;                  // no data in recv buffer
        memset(dataBuffer, 0, 1024);
        expectedBytesOnRead =  ((uint32_t) cmd->sectorCount) * 512;

        Debug::out(LOG_DEBUG, "justCmd: %02x, the data read phase will expect %d bytes", justCmd, expectedBytesOnRead);
    }

    // call one of the basic functions or try to call function from extension
    switch(cmd->functionId) {
        case CEX_FUN_OPEN:              cexOpen(); break;
        case CEX_FUN_GET_RESPONSE:      cexStatusOrResponse(cmd->extensionId, cmd->arg1, cmd->sectorCount); break;
        case CEX_FUN_CLOSE:             cexClose(cmd->extensionId); break;
        default:                        cexExtensionFunction(justCmd, cmd->extensionId, cmd->functionId, cmd->sectorCount); break;
    }

    Debug::out(LOG_DEBUG, "ExtensionHandler::processCommand -- send data and status after handling");

    if(expectedBytesOnRead > 0) {       // if we should fill bytes to expected size on read
        dataTrans->addZerosUntilSize(expectedBytesOnRead);
    }

    dataTrans->sendDataAndStatus();                 // send all the stuff after handling, if we got any
}

/*
    Open extension by name or by url.

    @param extName Name of extension, possibly with version (e.g. myExtension_1.0)
    @param extUrl Url where the extension can be retrieved when not currently installed.
    @return extension id (handle) - use it for accessing methods from this extension
*/
void ExtensionHandler::cexOpen(void)
{
    EXT_MUTEX_LOCK;

    char* extName = (char*) dataBuffer + 6;
    char* extUrl = (char*) (dataBuffer + 32 + 6);

    // check if extension is already running, reuse if it is
    for(int i=0; i<MAX_EXTENSIONS_OPEN; i++) {
        if(extensions[i].nameMatching(extName)) {   // supplied name matches one of our open extensions?
            extensions[i].touch();
            Debug::out(LOG_DEBUG, "ExtensionHandler::cexOpen - extension %s found at index %d", extName, i);
            dataTrans->setStatus(i);                // return index of this extension
            EXT_MUTEX_UNLOCK;
            return;
        }
    }

    // extension not running, find a slot for it
    int emptyIndex = -1;
    int oldestIndex = -1;
    uint32_t oldestAccessTime = 0xffffffff;

    for(int i=0; i<MAX_EXTENSIONS_OPEN; i++) {
        if(extensions[i].state == EXT_STATE_NOT_RUNNING && emptyIndex != -1) {  // found empty slot and didn't store empty slot before
            emptyIndex = i;
        }

        if(extensions[i].lastAccessTime < oldestAccessTime) {   // found older access time than in previous cycle? store index
            oldestAccessTime = extensions[i].lastAccessTime;
            oldestIndex = i;
        }
    }

    uint8_t extensionId = (emptyIndex != -1) ? emptyIndex : oldestIndex;    // use empty index if we got it, otherwise use oldest index
    Debug::out(LOG_DEBUG, "ExtensionHandler::cexOpen - will use index %d for extension %s", extensionId, extName);

    Extension* ext = &extensions[extensionId];              // use pointer to extension for simpler reading

    if(ext->state != EXT_STATE_NOT_RUNNING) {               // if the extension is running or starting, stop it now
        Debug::out(LOG_DEBUG, "ExtensionHandler::cexOpen - previous extension at index %d starting / running, so stoping it now", extensionId);
        cexClose(extensionId);
    }

    ext->clear();                        // clear everything
    ext->storeNameUrl(extName, extUrl);  // store name and url
    ext->touch();                        // update lastAccessTime

    // find path to start script
    std::string startScriptPath;
    ext->getStartScriptPath(startScriptPath);   // get /ce/extensions/my_extension/start.sh

    std::string startScriptCommand = startScriptPath;
    startScriptCommand += " ";

    char bfr[32];
    std::string extHandlerSockPath;
    getExtensionHandlerSockPath(extHandlerSockPath);    // get path to this handler's socket, where the extension should send responses
    startScriptCommand += extHandlerSockPath;           // cmd like: /ce/extensions/my_extension/start.sh /var/run/ce/extensions.sock
    startScriptCommand += " ";
    startScriptCommand += intToStr(extensionId, bfr);   // cmd like: /ce/extensions/my_extension/start.sh /var/run/ce/extensions.sock 1
    startScriptCommand += " &";                         // run command detached to not wait for it to terminate

    Debug::out(LOG_DEBUG, "ExtensionHandler::cexOpen - startScriptPath: '%s', startScriptCommand: '%s'", startScriptPath.c_str(), startScriptCommand.c_str());

    // start script exists, start it immediatelly
    if(Utils::fileExists(startScriptPath)) {
        ext->state = EXT_STATE_STARTING;        // mark this extension as starting

        Debug::out(LOG_DEBUG, "ExtensionHandler::cexOpen - now executing '%s'", startScriptCommand.c_str());
        system(startScriptCommand.c_str());     // execute the command
        dataTrans->setStatus(extensionId);      // return index of this extension
        EXT_MUTEX_UNLOCK;
        return;
    }

    // so the start script doesn't exist, assuming it's not downloaded and installed, so doing this now
    std::string jsonRequest = "{\"action\": \"install_extension\", \"url\": \"";
    jsonRequest += extUrl;                      // download / copy from here
    jsonRequest += "\", \"destination\": \"";

    std::string installPath;
    ext->getInstallPath(installPath);
    jsonRequest += installPath;                 // install here

    jsonRequest += "\", \"start_cmd\": \"";
    jsonRequest += startScriptCommand;          // run this command after install
    jsonRequest += "\"}";

    Debug::out(LOG_DEBUG, "ExtensionHandler::cexOpen - start script does not exist, so sending JSON: '%s' to TaskQ - for extension installation", jsonRequest.c_str());

    std::string taskQsockPath = Utils::dotEnvValue("TASKQ_SOCK_PATH");      // path to TaskQ socket
    bool ok = sendDataToSocket(taskQsockPath.c_str(), (uint8_t*) jsonRequest.c_str(), jsonRequest.length());       // try to send data

    if(ok) {    // sending ok?
        Debug::out(LOG_DEBUG, "Sending to TaskQ ok, marking extension state as EXT_STATE_INSTALLING");
        ext->state = EXT_STATE_INSTALLING;
        dataTrans->setStatus(extensionId);          // return index of this extension
    } else {    // sending failed?
        Debug::out(LOG_WARNING, "Sending to TaskQ failed, extension not started. Returning STATUS_EXT_ERROR.");
        ext->clear();
        dataTrans->setStatus(STATUS_EXT_ERROR);     // failed to send data means failed to start
    }

    EXT_MUTEX_UNLOCK;
}

/*
    Function will return whole response or just status byte (depending on sector count).
    If response is not available, will wait a little and then try again.
    Can return STATUS_NO_RESPONSE if response not available after that.
*/
void ExtensionHandler::cexStatusOrResponse(uint8_t extensionId, uint8_t functionId, uint8_t sectorCount)
{
    Debug::out(LOG_DEBUG, "ExtensionHandler::cexStatusOrResponse - extensionId: %d, functionId: %d, sectorCount: %d", extensionId, functionId, sectorCount);

    if(extensionId >= MAX_EXTENSIONS_OPEN) {        // bad extension id?
        Debug::out(LOG_WARNING, "ExtensionHandler::cexStatusOrResponse - extension index %d out of bounds", extensionId);
        dataTrans->setStatus(STATUS_BAD_ARGUMENT);
        return;
    }

    EXT_MUTEX_LOCK;
    Extension* ext = &extensions[extensionId];  // use pointer to extension for simpler reading
    ext->touch();

    uint32_t sendDataSize = sectorCount * 512;  // how many bytes ST is ready to get (based on sector count)

    if(functionId == CEX_FUN_OPEN) {            // if trying to get status of this extension - if it's open, then...
        if(ext->state == EXT_STATE_RUNNING) {   // if extesion is running, send function table
            uint32_t dataLen = ext->functionTable.exportBinarySignatures(dataBuffer);     // export signatures in binary form
            Debug::out(LOG_DEBUG, "ExtensionHandler::cexStatusOrResponse - signatures have %d bytes, will send %d bytes (based on sector count)", dataLen, sendDataSize);
            dataTrans->addDataBfr(dataBuffer, sendDataSize, true);
            dataTrans->setStatus(STATUS_OK);
        } else {                                // extension not running yet
            Debug::out(LOG_DEBUG, "ExtensionHandler::cexStatusOrResponse - extension not running, returning STATUS_EXT_NOT_RUNNING");
            dataTrans->setStatus(STATUS_EXT_NOT_RUNNING);
        }

        EXT_MUTEX_UNLOCK;
        return;
    }

    if(functionId <= CEX_FUN_CLOSE) {           // for fixed (base) functions return not/running status
        uint8_t openCloseStatus = (ext->state == EXT_STATE_RUNNING) ? STATUS_OK : STATUS_EXT_NOT_RUNNING;    // if running, return OK (0), otherwise returning NOT_RUNNING
        Debug::out(LOG_DEBUG, "ExtensionHandler::cexStatusOrResponse - functionId: %d - returning status: %02x", functionId, openCloseStatus);
        dataTrans->setStatus(openCloseStatus);
        EXT_MUTEX_UNLOCK;
        return;
    }

    Response* resp = &ext->response;

    // The following lines are for supporting multiple responses and their identification by funcCallId, but currently it's not working
    // if(resp->funcCallId != funcCallId) {     // bad functionId?
    //     Debug::out(LOG_WARNING, "ExtensionHandler::cexStatusOrResponse - no response stored for functionId %d (either not called yet, or rewritten by next function call)", functionId);
    //     dataTrans->setStatus(STATUS_BAD_ARGUMENT);
    //     EXT_MUTEX_UNLOCK;
    //     return;
    // }

    if(resp->state == RESP_STATE_NOT_RECEIVED) {    // if response not received yet, wait for it
        ExtensionHandler::clearSignal();            // if signal is set from previous event, just clear it now
        Debug::out(LOG_DEBUG, "ExtensionHandler::cexStatusOrResponse - response not received yet, will now wait");
        EXT_MUTEX_UNLOCK;            // unlock before waiting, so the other thread can access data
        ExtensionHandler::waitForSignal(500);       // waitForResponse - up to 500 ms
        EXT_MUTEX_LOCK;              // we're going to access the shared data now
    }

    if(resp->state == RESP_STATE_RECEIVED) {        // got the response now? return it
        uint32_t sendResponseSize = MIN(sendDataSize, resp->dataLen);   // pick the smaller of these two, because ST is expecting only sendDataSize

        if(sendResponseSize < resp->dataLen) {      // not sending whole response - response will be truncated?
            Debug::out(LOG_WARNING, "ExtensionHandler::cexStatusOrResponse - response size is %d bytes, but will send only %d bytes to ST - response truncated", resp->dataLen, sendResponseSize);
        } else {
            Debug::out(LOG_WARNING, "ExtensionHandler::cexStatusOrResponse - will return data response of %d bytes and status %02x", sendResponseSize, resp->statusByte);
        }

        dataTrans->addDataBfr(resp->data, sendResponseSize, true);
        dataTrans->setStatus(resp->statusByte);     // pass status byte
    } else {                                        // response not received yet
        dataTrans->setStatus(STATUS_NO_RESPONSE);
        Debug::out(LOG_DEBUG, "ExtensionHandler::cexStatusOrResponse - response not received yet");
    }

    EXT_MUTEX_UNLOCK;
}

/*
    Close extension identified by extension id.
*/
void ExtensionHandler::cexClose(uint8_t extensionId)
{
    if(extensionId >= MAX_EXTENSIONS_OPEN) {        // index out of range
        Debug::out(LOG_WARNING, "ExtensionHandler::cexClose - extension index %d out of bounds", extensionId);
        dataTrans->setStatus(STATUS_BAD_ARGUMENT);
        return;
    }

    EXT_MUTEX_LOCK;

    Extension* ext = &extensions[extensionId];  // use pointer to extension for simpler reading

    if(ext->state == EXT_STATE_NOT_RUNNING) {    // if it's not running
        Debug::out(LOG_DEBUG, "ExtensionHandler::cexClose - extension at index %d not running, NO OP", extensionId);
        ext->clear();
        dataTrans->setStatus(STATUS_OK);
        EXT_MUTEX_UNLOCK;
        return;
    }

    // send CLOSE request via socket
    sendCloseToExtension(extensionId);

    // find path to stop script
    std::string stopScriptCommand;
    ext->getStopScriptPath(stopScriptCommand);   // get /ce/extensions/my_extension/stop.sh

    if(!Utils::fileExists(stopScriptCommand)) {                 // stop script doesn't exist? fail here
        Debug::out(LOG_WARNING, "ExtensionHandler::cexClose - extension at index %d - stop script %s does not exist!", extensionId, stopScriptCommand.c_str());
        ext->clear();
        dataTrans->setStatus(STATUS_EXT_ERROR);
        EXT_MUTEX_UNLOCK;
        return;
    }

    stopScriptCommand += " &";              // run command detached to not wait for it to terminate
    Debug::out(LOG_DEBUG, "ExtensionHandler::cexClose - extension at index %d - now executing %s", extensionId, stopScriptCommand.c_str());
    system(stopScriptCommand.c_str());      // execute the command

    ext->clear();                           // clean up all the internal vars holding function table, function call statuses and responses
    dataTrans->setStatus(STATUS_OK);
    EXT_MUTEX_UNLOCK;
}

void ExtensionHandler::cexExtensionFunction(uint8_t justCmd, uint8_t extensionId, uint8_t functionId, uint32_t sectorCount)
{
    if(functionId <= CEX_FUN_CLOSE) {       // if trying to call this method with one of the base / fixed functions, fail
        Debug::out(LOG_WARNING, "ExtensionHandler::cexExtensionFunction - functionId %d too low for cexExtensionFunction", functionId);
        dataTrans->setStatus(STATUS_BAD_ARGUMENT);
        return;
    }

    int functionIndex = functionId - (CEX_FUN_CLOSE + 1);       // turn passed in function id 3 to exported function index 0

    if(extensionId >= MAX_EXTENSIONS_OPEN) {        // bad extension id?
        Debug::out(LOG_WARNING, "ExtensionHandler::cexExtensionFunction - extension index %d out of bounds", extensionId);
        dataTrans->setStatus(STATUS_BAD_ARGUMENT);
        return;
    }

    EXT_MUTEX_LOCK;
    Extension* ext = &extensions[extensionId];      // use pointer to extension for simpler reading
    ext->dumpToLog();

    if(ext->state != EXT_STATE_RUNNING) {           // extension not running?
        Debug::out(LOG_WARNING, "ExtensionHandler::cexExtensionFunction - extension at index %d NOT RUNNING", extensionId);
        dataTrans->setStatus(STATUS_EXT_NOT_RUNNING);
        EXT_MUTEX_UNLOCK;
        return;
    }

    ext->touch();

    if(functionIndex > MAX_EXPORTED_FUNCTIONS || !ext->functionTable.signatures[functionIndex].used) {
        Debug::out(LOG_WARNING, "ExtensionHandler::cexExtensionFunction - functionIndex %d is not used (expoted) or functionIndex %d > %d", functionIndex, functionIndex, MAX_EXPORTED_FUNCTIONS);
        dataTrans->setStatus(STATUS_BAD_ARGUMENT);
        EXT_MUTEX_UNLOCK;
        return;
    }

    FunctionSignature* sign = &ext->functionTable.signatures[functionIndex];        // use pointer to signature for simpler reading
    uint8_t expectedJustCmd = sign->getAcsiCmdForFuncType();                // what justCmd must match for this function
    if(justCmd != expectedJustCmd) {        // wrong CMD used? fail
        Debug::out(LOG_WARNING, "ExtensionHandler::cexExtensionFunction - expected cmd is %02x, but cmd that came is %02x", expectedJustCmd, justCmd);
        dataTrans->setStatus(STATUS_BAD_ARGUMENT);
        EXT_MUTEX_UNLOCK;
        return;
    }

    // resolve functionId / functionIndex to functionName
    char functionName[32];
    sign->getName(functionName, sizeof(functionName));
    Debug::out(LOG_DEBUG, "ExtensionHandler::cexExtensionFunction  extensionId: %d, functionId: %d, functionIndex: %d, functionName: '%s'", extensionId, functionId, functionIndex, functionName);

    if(justCmd == CMD_CALL_RAW_WRITE || justCmd == CMD_CALL_RAW_READ) {     // raw WRITE and raw READ
        Debug::out(LOG_DEBUG, "ExtensionHandler::cexExtensionFunction - justCmd: %d, extensionId: %d, will do raw call of function '%s'", justCmd, extensionId, functionName);

        sendCallRawToExtension(extensionId, functionName);
        EXT_MUTEX_UNLOCK;        // unlock mutex now, because cexStatusOrResponse() will do own lock and unlock

        // if it's WRITE, there will be no data read, so return only STATUS. For read return data + status == full response.
        uint8_t responseSectorCount = (justCmd == CMD_CALL_RAW_READ) ? sectorCount : 0;
        cexStatusOrResponse(extensionId, functionId, responseSectorCount);
    } else if(justCmd == CMD_CALL_LONG_WRITE_ARGS) {                        // long call - write args to extension
        Debug::out(LOG_DEBUG, "ExtensionHandler::cexExtensionFunction - justCmd: %d, extensionId: %d, will do LONG WRITE ARGS call of function '%s'", justCmd, extensionId, functionName);

        sendCallLongToExtension(extensionId, functionIndex, functionName);
        EXT_MUTEX_UNLOCK;        // unlock mutex now, because cexStatusOrResponse() will do own lock and unlock

        cexStatusOrResponse(extensionId, functionId, 0);                    // no returning sector count
    } else {                                                                // something else? fail
        Debug::out(LOG_WARNING, "ExtensionHandler::cexExtensionFunction - bad justCmd %d passed to this method", justCmd);
        dataTrans->setStatus(STATUS_BAD_ARGUMENT);
        EXT_MUTEX_UNLOCK;
    }
}

// used in CMD_CALL_RAW_WRITE and CMD_CALL_RAW_READ to do the extension function call 
uint8_t ExtensionHandler::sendCallRawToExtension(uint8_t extensionId, char* functionName)
{
    // first send data to extension if some data was received (WRITE cmd) (6 is just our header, so if it's only 6, then there's no data)
    if(byteCountInDataBuffer > 6) {
        sendDataToExtension(extensionId, dataBuffer, byteCountInDataBuffer);
    }

    // format function name, arguments to JSON
    char bfr[32];

    std::string funcCallJson;
    funcCallJson += "{\"function\": \"";
    funcCallJson += functionName;
    funcCallJson += "\", \"args\": [";
    funcCallJson += intToStr(cmd4, bfr);
    funcCallJson += ", ";
    funcCallJson += intToStr(cmd5, bfr);
    funcCallJson += "]}";

    Extension* ext = &extensions[extensionId];      // use pointer to extension for simpler reading
    ext->response.updateBeforeSend();               // update response internals before sending out function call

    // now send the JSON with function name and args to extension
    sendStringToExtension(extensionId, funcCallJson.c_str());
    Debug::out(LOG_DEBUG, "ExtensionHandler::sendCallRawToExtension - funcCallJson: '%s'", funcCallJson.c_str());

    return ext->response.funcCallId;     // return this function call id
}

// used in CMD_CALL_LONG_WRITE_ARGS to do the extension function call 
uint8_t ExtensionHandler::sendCallLongToExtension(uint8_t extensionId, uint8_t functionIndex, char* functionName)
{
    Debug::out(LOG_DEBUG, "ExtensionHandler::sendCallLongToExtension - extensionId: %d, functionIndex: %d, functionName: '%s'", extensionId, functionIndex, functionName);

    std::string funcCallJson;
    funcCallJson += "{\"function\": \"";
    funcCallJson += functionName;
    funcCallJson += "\", \"args\": [";

    Extension* ext = &extensions[extensionId];  // use pointer to extension for simpler reading

    // based on the exported functions table, extract values and convert them to JSON. If argument is path, do translation
    FunctionSignature *fs = &ext->functionTable.signatures[functionIndex];
    uint8_t* param = dataBuffer + 6;                        // the real data start with offset of 6 bytes

    bool gotArgument = false;       // don't add column before 1st argument
    char bfr[32];

    for(int i=0; i<MAX_FUNCTION_ARGUMENTS; i++) {
        if(fs->argumentTypes[i] == TYPE_NOT_PRESENT) {      // we found unused argument == end of arguments
            break;
        }

        if(gotArgument) {           // if some argument was already stored, add column between prev and this argument
            funcCallJson += ", ";
        }
        gotArgument = true;         // in the next loop do add column
        int len = 0;
        uint32_t binDataLen = 0;

        switch(fs->argumentTypes[i]) {
            case TYPE_UINT8:        funcCallJson += intToStr((uint32_t) *param, bfr);                       param += 1; break;
            case TYPE_UINT16:       funcCallJson += intToStr((uint32_t) Utils::getWord(param), bfr);        param += 2; break;
            case TYPE_UINT32:       funcCallJson += intToStr((uint32_t) Utils::getDword(param), bfr);       param += 4; break;
            case TYPE_INT8:         funcCallJson += intToStr((int32_t) *param, bfr);                        param += 1; break;
            case TYPE_INT16:        funcCallJson += intToStr((int32_t) Utils::getWord(param), bfr);         param += 2; break;
            case TYPE_INT32:        funcCallJson += intToStr((int32_t) Utils::getDword(param), bfr);        param += 4; break;

            case TYPE_CSTRING:      // zero terminated string
            {
                len = strlen((char*) param);
                funcCallJson += "\"";
                funcCallJson += (char*) param;
                funcCallJson += "\"";
                param += len + 1;   // move after the string and it's terminating zero
                break;
            }

            case TYPE_PATH:
            {
                len = strlen((char*) param);

                // param points now to short Atari path, we need to convert it
                funcCallJson += "\"";

                std::string shortPath = (char*) param;
                std::string outFullHostPath;
                ldp_shortToLongPath(shortPath, outFullHostPath, true, NULL);    // short path to long path

                funcCallJson += outFullHostPath;        // store converted path here, not the original one
                funcCallJson += "\"";

                param += len + 1;   // move after the string and it's terminating zero
                break;
            }

            case TYPE_BIN_DATA:
            {
                binDataLen = Utils::getDword(param);    // 4 bytes - binary data length

                dataBuffer2[0] = 'D';                   // this buffer has data and DA is the marker
                dataBuffer2[1] = 'A';

                uint32_t binDataLenVerified = MIN(binDataLen, EXT_BUFFER_SIZE - 6);
                Debug::out(LOG_DEBUG, "ExtensionHandler::sendCallLongToExtension - binDataLen: %d, EXT_BUFFER_SIZE: %d, using smaller: %d", binDataLen, EXT_BUFFER_SIZE-6, binDataLenVerified);

                Utils::storeDword(dataBuffer2 + 2, binDataLenVerified);     // bytes 2,3,4,5
                memcpy(dataBuffer2 + 6, param + 4, binDataLenVerified);     // copy over data to bytes 6 .. (binDataLenVerified+6)
                sendDataToExtension(extensionId, dataBuffer2, binDataLenVerified + 6);  // send data + 6 bytes of header 

                param += binDataLen + 4;                // advance by data size + the length size (4 bytes)
                break;
            }
        }
    }

    funcCallJson += "]}";       // terminate arguments and JSON
    Debug::out(LOG_DEBUG, "ExtensionHandler::sendCallLongToExtension - funcCallJson: '%s'", funcCallJson.c_str());

    ext->response.updateBeforeSend();   // update response internals before sending out function call

    // now send the JSON with function name and args to extension
    sendStringToExtension(extensionId, funcCallJson.c_str());

    return ext->response.funcCallId;
}

void ExtensionHandler::sendCloseToExtension(uint8_t extensionId)
{
    sendStringToExtension(extensionId, "{\"function\": \"CEX_FUN_CLOSE\"}");
}

bool ExtensionHandler::sendStringToExtension(uint8_t extensionId, const char* str)
{
    return sendDataToExtension(extensionId, (const uint8_t*) str, strlen(str) + 1);
}

bool ExtensionHandler::sendDataToExtension(uint8_t extensionId, const uint8_t* data, uint32_t dataLen)
{
    return sendDataToSocket(extensions[extensionId].outSocketPath, data, dataLen);
}

bool ExtensionHandler::sendDataToSocket(const char* socketPath, const uint8_t* data, uint32_t dataLen)
{
	// create a UNIX DGRAM socket
	int sockFd = socket(AF_UNIX, SOCK_DGRAM, 0);

	if (sockFd < 0) {   // if failed to create socket
	    Debug::out(LOG_ERROR, "ExtensionHandler::sendDataToSocket: failed to create socket - errno: %d", errno);
	    return false;
	}

    Debug::out(LOG_DEBUG, "ExtensionHandler::sendDataToSocket: opened socket %s", socketPath);

    struct sockaddr_un addr;
    strcpy(addr.sun_path, socketPath);
    addr.sun_family = AF_UNIX;

    // try to send to extension socket
    int res = sendto(sockFd, data, dataLen, 0, (struct sockaddr *) &addr, sizeof(struct sockaddr_un));

    if(res < 0) {       // if failed to send
	    Debug::out(LOG_ERROR, "ExtensionHandler::sendDataToSocket: sendto failed - errno: %d", errno);
    } else {
        Debug::out(LOG_DEBUG, "ExtensionHandler::sendDataToSocket: %d bytes of data sent", dataLen);
    }

    close(sockFd);
    return (res >= 0);
}

bool ExtensionHandler::isExtensionCall(uint8_t justCmd)
{
    if( justCmd == CMD_CALL_RAW_READ ||
        justCmd == CMD_CALL_RAW_WRITE ||
        justCmd == CMD_CALL_LONG_WRITE_ARGS ||
        justCmd == CMD_CALL_GET_RESPONSE) {
            return true;
        }

    return false;
}

const char *ExtensionHandler::getCommandName(uint8_t cmd)
{
    switch(cmd) {
        case CMD_CALL_RAW_READ:         return "CMD_CALL_RAW_READ";
        case CMD_CALL_RAW_WRITE:        return "CMD_CALL_RAW_WRITE";
        case CMD_CALL_LONG_WRITE_ARGS:  return "CMD_CALL_LONG_WRITE_ARGS";
        case CMD_CALL_GET_RESPONSE:     return "CMD_CALL_GET_RESPONSE";
        default:                        return "UNKNOWN";
    }
}

void ExtensionHandler::getExtensionHandlerSockPath(std::string& sockPath)
{
    sockPath = Utils::dotEnvValue("EXT_SOCK_PATH");
}

char* ExtensionHandler::intToStr(int arg, char* bfr)
{
    sprintf(bfr, "%d", arg);
    return bfr;
}
