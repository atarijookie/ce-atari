#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>

#include "extensiondefs.h"
#include "main.h"
#include "functions.h"
#include "json.h"

/*
This is an CosmosEx extension example in C++.
You can base your own extension based on this example, you just need to change
EXTENSION_NAME and also define your exported functions in functions.cpp file.
No other changes to this file are not needed (unless you need something special).

EXTENSION_NAME should match the directory name of this extension, so CE core will know
where to look for start.sh and stop.sh scripts.

Jookie, 2024
*/

volatile sig_atomic_t shouldStop = 0;
using json = nlohmann::json;

#define MAX_SOCKET_PATH     255
#define EXTENSION_NAME      "extension_example_c"           // <<< CHANGE THIS IN YOUR OWN EXTENSION TO SOMETHING ELSE
#define IN_SOCKET_PATH      "/tmp/" EXTENSION_NAME ".sock"  // where we will get commands from CE
char outSocketPath[MAX_SOCKET_PATH + 1];                // where we should send responses to commands
int extensionId = -1;                                   // CE knows this extension under this id (index), we must send it in responses
uint8_t latestData[EXT_BUFFER_SIZE];                    // will hold received binary data and raw data
uint32_t latestDataLen = 0;

int exportedFunctionsCount = 0;
ReceivedSignature exportedFunctions[MAX_EXPORTED_FUNCTIONS];    // all the exported functions signatures will be stored here
ExportedFunctionNameToPointer exportedFunNameToPointer[MAX_EXPORTED_FUNCTIONS]; // used to find pointer to function from function name

typedef void ExportedFunction_t(json args, ResponseFromExtension* resp);

int main(int argc, char *argv[])
{
    // make sure the correct number of arguments is specified
    if(argc != 3) {
        printf("\n\n%s -- Wrong number of arguments specified.\nUsage:\n", argv[0]);
        printf("%s /PATH/TO/SOCKET EXT_ID'\n", argv[0]);
        printf("- /PATH/TO/SOCKET - path to unix socket of CE, where the responses should be sent'\n");
        printf("- EXT_ID - extension id - id (index) under which the CE core has this extension stored'\n");
        printf("Terminated.\n\n");
        return 1;
    }

    strncpy(outSocketPath, argv[1], MAX_SOCKET_PATH); // copy in the out sock path
    outSocketPath[MAX_SOCKET_PATH - 1] = 0;           // zero terminate, just to be sure

    sscanf(argv[2], "%d", &extensionId);            // read this extension id (index) from the argument

    int sock = createRecvSocket(IN_SOCKET_PATH);    // create socket where we will get requests from CE core

    if(sock == -1) {                    // if failed to create socket, quit
        return 1;
    }

    // get all the signatures into the exportedFunctions array for usage in sendExportedFunctionsTable() and in every function call
    exportFunctionSignatures();

    printf("Sending exported function signatures\n");
    sendExportedFunctionsTable();       // let CE core know that this extension has started

    printf("Entering main loop, waiting for messages via: %s\n", IN_SOCKET_PATH);

    uint8_t bfr[EXT_BUFFER_SIZE];

    while(!shouldStop) {
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

        ssize_t recvCnt = recv(sock, bfr, sizeof(bfr), 0);  // receive now

        if(recvCnt < 2) {           // received data too short?
            printf("received message too short, ignoring message\n");
            continue;
        }

        bfr[recvCnt] = 0;           // zero terminate the received data)

        printf("received size: %d\n", (int) recvCnt);
        // printf("received data: %s\n", bfr);  // show all the received data as-is on console

        if(bfr[0] == 'D' && bfr[1] == 'A') {    // data starting with 'DA' - it's raw data
            handleRawData(bfr);
        } else if(bfr[0] == '{') {              // data starting with '{' - it's JSON message
            handleJsonMessage(bfr);
        } else {                    // some other start of packet, ignore it
            printf("unknown message start, ignoring message");
            continue;
        }
    }

    sendClosedNotification();       // tell the CE core that we're closing

    close(sock);
    printf("Terminated...\n");
    return 0;
}

int createRecvSocket(const char* pathToSocket)
{
	// create a UNIX DGRAM socket
	int sock = socket(AF_UNIX, SOCK_DGRAM, 0);

	if (sock < 0) {
	    printf("createRecvSocket - failed to create socket!\n");
	    return -1;
	}

    fchmod(sock, S_IRUSR | S_IWUSR);        // restrict permissions before bind

    unlink(pathToSocket);                   // delete sock file if exists

    struct sockaddr_un addr;
    strcpy(addr.sun_path, pathToSocket);
    addr.sun_family = AF_UNIX;

    int res = bind(sock, (struct sockaddr *) &addr, strlen(addr.sun_path) + sizeof(addr.sun_family));
    if (res < 0) {
	    printf("createRecvSocket - failed to bind socket to %s - errno: %d\n", pathToSocket, errno);
	    return -1;
    }

    chmod(addr.sun_path, 0666);             // loosen permissions

    printf("createRecvSocket - %s created, sock: %d\n", pathToSocket, sock);
    return sock;
}

bool sendDataToSocket(const char* socketPath, const uint8_t* data, uint32_t dataLen)
{
	// create a UNIX DGRAM socket
	int sockFd = socket(AF_UNIX, SOCK_DGRAM, 0);

	if (sockFd < 0) {   // if failed to create socket
	    printf("sendDataToSocket: failed to create socket - errno: %d\n", errno);
	    return false;
	}

    // printf("sendDataToSocket: opened socket %s\n", socketPath);

    struct sockaddr_un addr;
    strcpy(addr.sun_path, socketPath);
    addr.sun_family = AF_UNIX;

    // try to send to extension socket
    int res = sendto(sockFd, data, dataLen, 0, (struct sockaddr *) &addr, sizeof(struct sockaddr_un));

    if(res < 0) {       // if failed to send
	    printf("sendDataToSocket: sendto failed - errno: %d\n", errno);
    } else {
        // printf("sendDataToSocket: %d bytes of data sent\n", dataLen);
    }

    close(sockFd);
    return (res >= 0);
}

void addFunctionSignature(void* pFunc, const char* name, uint8_t fun_type, uint8_t* argumentTypes, uint8_t argumentTypesCount, uint8_t returnValueType)
{
    if(exportedFunctionsCount >= MAX_EXPORTED_FUNCTIONS) {   // cannot add more?
        printf("You have reached the maximum exported functions count - %d!", MAX_EXPORTED_FUNCTIONS);
        exit(1);
    }

    ReceivedSignature* sign = &exportedFunctions[exportedFunctionsCount];   // get pointer to current signature
    functionSignatureToBytes(name, fun_type, argumentTypes, argumentTypesCount, returnValueType, sign);     // fill signature with data

    exportedFunNameToPointer[exportedFunctionsCount].pFunc = pFunc;         // store pointer to function
    strncpy(exportedFunNameToPointer[exportedFunctionsCount].name, name, MAX_FUNCTION_NAME_LEN);    // store function name

    exportedFunctionsCount++;               // increment exported signatures count
}

void functionSignatureToBytes(const char* name, uint8_t fun_type, uint8_t* argumentTypes, uint8_t argumentTypesCount, uint8_t returnValueType, ReceivedSignature* signature)
{
    if(argumentTypesCount > MAX_FUNCTION_ARGUMENTS) {
        printf("Exported function name %s has %d arguments, but only %d are allowed. This will not work. Use less arguments!", name, argumentTypesCount, MAX_FUNCTION_ARGUMENTS);
        exit(1);
    }

    memset(signature, 0, sizeof(ReceivedSignature));            // set everything to zeros
    strncpy(signature->name, name, MAX_FUNCTION_NAME_LEN - 1);  // copy in name
    signature->funcType = fun_type;                             // store function type
    signature->argumentsCount = argumentTypesCount;             // store count of arguments
    memcpy(signature->argumentTypes, argumentTypes, argumentTypesCount);    // copy in the argument types

    if(returnValueType < TYPE_NOT_PRESENT || returnValueType > TYPE_BIN_DATA) { // invalid type?
        printf("Invalid return value type %d. This will not work. Use some of the TYPE_* values!", returnValueType);
        exit(1);
    }

    signature->returnValueType = returnValueType;               // store return value type
}

// Function to formulate response in expected format and send it to CE core.
void sendResponse(const char* funName, uint8_t status, uint8_t* data, uint32_t dataLen)
{
    printf("sendResponse - funName: %s, status: %d, dataLen: %d\n", funName, status, dataLen);

    ResponseFromExtension resp;
    responseInit(&resp, funName);
    responseStoreStatusAndDataLen(&resp, status, dataLen);

    if(dataLen > 0) {
        memcpy(resp.data, data, dataLen);       // copy in data
    }

    sendDataToSocket(outSocketPath, (uint8_t*) &resp, RESPONSE_HEADER_SIZE + dataLen);     // send to CE core
}

void responseStoreStatusAndDataLen(ResponseFromExtension* resp, uint8_t status, uint32_t dataLen)
{
    resp->statusByte = status;                  // store status byte
    responseStoreDataLen(resp, dataLen);        // store data length with expected endiannes 
}

void responseStoreDataLen(ResponseFromExtension* resp, uint32_t dataLen)
{
    if(dataLen > MAX_RESPONSE_DATA_SIZE) {      // dataLen too big?
        printf("Invalid dataLen %d. This will not work. Maximum allowed size is: %d", dataLen, MAX_RESPONSE_DATA_SIZE);
        exit(1);
    }

    storeDword(resp->dataLen, dataLen);         // store data length with expected endiannes 
}

void responseInit(ResponseFromExtension* resp, const char* funName)
{
    memset(resp, 0, RESPONSE_HEADER_SIZE);                          // clear the reponse header
    resp->extensionId = extensionId;                                // extension id
    strncpy(resp->functionName, funName, MAX_FUNCTION_NAME_LEN -1); // function name
}

uint16_t getWord(uint8_t *bfr)
{
    uint16_t val = 0;

    val = bfr[0];       // get hi
    val = val << 8;

    val |= bfr[1];      // get lo

    return val;
}

uint32_t getDword(uint8_t *bfr)
{
    uint32_t val = 0;

    val = bfr[0];       // get hi
    val = val << 8;

    val |= bfr[1];      // get mid hi
    val = val << 8;

    val |= bfr[2];      // get mid lo
    val = val << 8;

    val |= bfr[3];      // get lo

    return val;
}

void storeWord(uint8_t *bfr, uint16_t val)
{
    bfr[0] = val >> 8;  // store hi
    bfr[1] = val;       // store lo
}

void storeDword(uint8_t *bfr, uint32_t val)
{
    bfr[0] = val >> 24; // store hi
    bfr[1] = val >> 16; // store mid hi
    bfr[2] = val >>  8; // store mid lo
    bfr[3] = val;       // store lo
}

/*
After the start of this extension we will send signatures of exported functions
to CE core, so the CE core will know that this extension has started and what function it exports.
*/
void sendExportedFunctionsTable(void)
{
    #define MAX_EXPORTED_FUNCTIONS_SIZE (MAX_EXPORTED_FUNCTIONS * sizeof(ReceivedSignature))
    #define BUFFER_SIZE                 (MAX_EXPORTED_FUNCTIONS_SIZE + MAX_SOCKET_PATH + 1)

    uint8_t outBuffer[BUFFER_SIZE];
    memset(outBuffer, 0, BUFFER_SIZE);          // clear buffer before filling it

    int exportedFunctionsSizeInBytes = exportedFunctionsCount * sizeof(ReceivedSignature);
    memcpy(outBuffer, exportedFunctions, exportedFunctionsSizeInBytes);         // copy the exported function signatures to output buffer

    char* pBeyondFuncs = (char*) (outBuffer + exportedFunctionsSizeInBytes);    // pointer to place beyond the exported functions
    strncpy(pBeyondFuncs, IN_SOCKET_PATH, MAX_SOCKET_PATH);     // copy in socket path beyond the exported functions

    uint32_t openRespLen = exportedFunctionsSizeInBytes + strlen(IN_SOCKET_PATH) + 1;
    sendResponse("CEX_FUN_OPEN", exportedFunctionsCount, outBuffer, openRespLen);   // exported functions count instead of OK status
}

void sendClosedNotification(void)
{
    // send also extension name, so core won't accidentally mark other extension closed
    sendResponse("CEX_FUN_CLOSE", STATUS_OK, (uint8_t*) EXTENSION_NAME, strlen(EXTENSION_NAME) + 1);
}

/*
Handle RAW data from CE - sent when extension uses RAW call, or sends TYPE_BIN_DATA buffer.
As we don't want to encode binary data to be sendable in JSON, we use first packet with
raw binary data, which we store, and right after this packet the CE core will send
one more packet with the JSON message, which will then use the data received here.
*/
void handleRawData(uint8_t* data)
{
    latestDataLen = getDword(data + 2);                     // get supplied data length
    latestDataLen = MIN(latestDataLen, EXT_BUFFER_SIZE);    // limit data length to maximum buffer size
    memcpy(latestData, data + 6, latestDataLen);            // copy in the data
}

/*
Handle JSON message from CE - used for calling exported functions (even the raw ones).
When RAW WRITE message is received, then the previously raw binary data will be appended to arguments.
*/
void handleJsonMessage(uint8_t* bfr)
{
    json data;
    try {
        data = json::parse(bfr);   // try to parse the message
    }
    catch(...)                     // on any exception - log it, don't crash
    {
        std::exception_ptr p = std::current_exception();
        printf("json::parse raised an exception: %s\n", (p ? p.__cxa_exception_type()->name() : "null"));
        return;
    }

    // std::string s = data.dump();
    // printf("received json message: %s\n", s.c_str());    // show received json message after parsing

    if(!data.contains("function")) {    // the function name must be present in received json message
        printf("'function' missing in received message.\n");
        return;
    }

    std::string funName = data["function"].get<std::string>();

    if(funName.compare("CEX_FUN_CLOSE") == 0) {     // when this was a request to terminate this extension
        printf("Received command from CE that we should terminate, so terminating now.\n");
        shouldStop = 1;
        return;
    }

    ReceivedSignature* sign = getExportedFunctionSignature(funName.c_str());

    if(!sign) {         // failed to find function signature by name?
        printf("function '%s' not found in exported functions signatures\n", funName.c_str());
        return;
    }

    bool is_raw_write = sign->funcType == FUNC_RAW_WRITE;   // True if this is raw write

    void* pFunc = NULL;
    for(int i=0; i<exportedFunctionsCount; i++) {       // find pointer to the wanted function by function name
        if(strcmp(exportedFunNameToPointer[i].name, funName.c_str()) == 0) {
            pFunc = exportedFunNameToPointer[i].pFunc;
            break;
        }
    }

    if(!pFunc) {        // function pointer not found? quit
        printf("Could not find function '%s', message not handled\n", funName.c_str());
        return;
    }

    ResponseFromExtension resp;             // response will be stored here
    responseInit(&resp, funName.c_str());   // init header, copy name in

    printf("calling extension function '%s'\n", funName.c_str());

    json emptyArray;
    json args = data.contains("args") ? data["args"] : emptyArray;      // if args are present in JSON, use them; otherwise use empty array

    // In case there are some function arguments missing, we're just adding zero arguments instead of them.
    // We could also reject the message in this case, but it might be more helpful to help getting the message through.
    // But do warn that this happened, as this might cause other issues.
    int missingArgs = sign->argumentsCount - args.size();
    if(missingArgs > 0) {                   // some args are missing compared to expected args count from signature?
        printf("the received message is missing %d arguments, padding with zeros\n", missingArgs);  // at least warn that this happened
        for(int i=0; i<missingArgs; i++) {  // fill missing args with zeros
            args.push_back(0);
        }
    }

    ((ExportedFunction_t*) pFunc) (args, &resp);    // call the function

    if(is_raw_write) {                      // if this is a RAW WRITE, we always return just status (never data)
        responseStoreDataLen(&resp, 0);
    }

    if(sign->returnValueType == RESP_TYPE_NONE) {   // when there should be no response from function, always return OK and no data
        responseStoreStatusAndDataLen(&resp, STATUS_OK, 0);
    }

    uint32_t dataLen = getDword(resp.dataLen); // get expected length of response

    // send to CE core
    sendDataToSocket(outSocketPath, (uint8_t*) &resp, RESPONSE_HEADER_SIZE + dataLen);
}

// Look for a exported function signature that matches specified name
ReceivedSignature* getExportedFunctionSignature(const char* funName)
{
    for(int i=0; i<exportedFunctionsCount; i++) {               // go through all the exported function signatures we have
        if(strcmp(exportedFunctions[i].name, funName) == 0) {   // name matching? return pointer to this struct
            return &exportedFunctions[i];
        }
    }

    return NULL;    // function not found, return NULL
}
