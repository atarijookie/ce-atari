#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <stdarg.h>

#include "extensiondefs.h"
#include "extension.h"
#include "stdlib.h"

#define DONT_CHECK_ARG_COUNT    0xff

BinarySignatureForST signatures[MAX_EXPORTED_FUNCTIONS];

BinarySignatureForST signOpen = {
    CEX_FUN_OPEN,
    0x190e,                         // hash of "CEX_FUN_OPEN"
    FUNC_RAW_WRITE,                 // cmd that should be used
    0,                              // argumentsCount
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // no official args
    RESP_TYPE_STATUS
};

// Function for opening the extension by extensionName.
uint8_t cexOpen(char* extensionName, char* extensionSourceUrl, uint16_t timeoutSeconds)
{
    // copy params for OPEN function into buffer
    uint8_t* bfr = __getBuffer();
    memset(bfr, 0, 512);                    // clear it completely
    strcpy((char*) bfr, extensionName);             // 0-31: name of extnesion 
    strcpy((char*) (bfr + 32), extensionSourceUrl); // 32-rest: source url of extension

    CEXcall cc;
    cc.readNotWrite = DATA_DIR_WRITE;
    cc.cmd = FUNC_RAW_WRITE;                // ACSI command that will be used
    cc.extensionId = 0;                     // no extension id when opening
    cc.functionId = CEX_FUN_OPEN;
    cc.sectorCount = 1;                     // 1 sector to transfer
    cc.arg1 = 0;
    cc.arg2 = 0;
    cc.buffer = bfr;

    uint8_t extId = __doTheCall(&cc);       // send this to CE

    if(extId & 0xf0) {          // if the response to OPEN has something in the upper nibble, it's an error, return it
        return extId;
    }

    int i;
    for(i=0; i<timeoutSeconds; i++) {       // wait for extension to start until open or timeout
        uint8_t res = cexResponse(extId, "CEX_FUN_OPEN", 512, bfr);     // get status of opening the extension

        if(res == STATUS_OK) {              // extension open? stop waiting, return extension ID
            memcpy(signatures, bfr, MAX_EXPORTED_FUNCTIONS * sizeof(BinarySignatureForST));  // use the received signatures to fill the signatures[] array
            (void) Cconws("cexOpen OK\r\n");
            return extId;
        }

        (void) Cconws("cexOpen waiting for extension start.\r\n");
        sleep(1);
    }

    (void) Cconws("cexOpen timeout!\r\n");
    return STATUS_EXT_NOT_RUNNING;          // extension not running if got here
}

void cexClose(uint8_t extensionId)
{
    CEXcall cc;
    cc.readNotWrite = DATA_DIR_WRITE;
    cc.cmd = FUNC_RAW_WRITE;                // ACSI command that will be used
    cc.extensionId = extensionId;           // ID of extension
    cc.functionId = CEX_FUN_CLOSE;
    cc.sectorCount = 0;
    cc.arg1 = 0;
    cc.arg2 = 0;
    cc.buffer = __getBuffer();              // we're not expecting any data to be send, but supply buffer just to be sure

    __doTheCall(&cc);   // send this to CE
}

// Raw call of extension function. Faster but no arguments conversion done.
uint8_t cexCallRawWrite(uint8_t extensionId, char* functionName, uint8_t arg1, uint8_t arg2, uint32_t dataLenBytes, uint8_t* pData)
{
    BinarySignatureForST* sign = __validate(functionName, FUNC_RAW_WRITE, DONT_CHECK_ARG_COUNT);

    if(!sign) {     // validate failed? fail here
        return STATUS_BAD_ARGUMENT;
    }

    CEXcall cc;
    cc.readNotWrite = DATA_DIR_WRITE;
    cc.cmd = FUNC_RAW_WRITE;        // ACSI command that will be used
    cc.extensionId = extensionId;   // ID of extension
    cc.functionId = sign->index;    // function id is index in the signature table
    cc.sectorCount = __bytesLenToSectorsLen(dataLenBytes);      // bytes count to sector count
    cc.arg1 = arg1;                 // pass arg1 and arg2 as they are
    cc.arg2 = arg2;
    cc.buffer = pData;              // where the data is stored

    return __doTheCall(&cc);        // send this to CE
}

// Raw call of extension function. Faster but no arguments conversion done.
uint8_t cexCallRawRead(uint8_t extensionId, char* functionName, uint8_t arg1, uint8_t arg2, uint32_t dataLenBytes, uint8_t* pData)
{
    BinarySignatureForST* sign = __validate(functionName, FUNC_RAW_READ, DONT_CHECK_ARG_COUNT);

    if(!sign) {     // validate failed? fail here
        return STATUS_BAD_ARGUMENT;
    }

    CEXcall cc;
    cc.readNotWrite = DATA_DIR_READ;
    cc.cmd = FUNC_RAW_READ;         // ACSI command that will be used
    cc.extensionId = extensionId;   // ID of extension
    cc.functionId = sign->index;    // function id is index in the signature table
    cc.sectorCount = __bytesLenToSectorsLen(dataLenBytes);      // bytes count to sector count
    cc.arg1 = arg1;                 // pass arg1 and arg2 as they are
    cc.arg2 = arg2;
    cc.buffer = pData;              // where the data will be stored after this call

    return __doTheCall(&cc);        // send this to CE
}

// Call of extension function with longer arguments. Slower than the RAW options, but allows you to pass arguments nicely.
// Maximum allowed params size together is one sector size.
uint8_t cexCallLong(uint8_t extensionId, char* functionName, int argc, ...)
{
    BinarySignatureForST* sign = __validate(functionName, FUNC_LONG_ARGS, argc);

    if(!sign) {                 // validate failed? fail here
        return STATUS_BAD_ARGUMENT;
    }

    //---------
    // put args in the bufferForArgs based on signature
    uint8_t* bfrArgs = __getBuffer();   // args will be stored in this buffer
    uint8_t* bfr = bfrArgs;     // start placing args here

    va_list args;
    va_start(args, argc);       // start variable argument count access

    int i;
    for(i=0; i<argc; i++) {     // place args on buffer
        uint32_t oneArg = va_arg(args, uint32_t);   // get one arg
        switch(sign->argumentTypes[i]) {
            case TYPE_UINT8:
            case TYPE_INT8:
            {
                *bfr = oneArg;
                bfr++;
                break;
            }

            case TYPE_UINT16:
            case TYPE_INT16:
            {
                storeWord(bfr, oneArg);
                bfr += 2;
                break;
            }

            case TYPE_UINT32:
            case TYPE_INT32:
            {
                storeDword(bfr, oneArg);
                bfr += 4;
                break;
            }

            case TYPE_CSTRING:
            case TYPE_PATH:
            {
                int len = strlen((char*) oneArg);   
                strcpy((char*)bfr, (char*) oneArg);     // copy in the string
                bfr += len + 1;                         // advance beyond the string and terminating zero
                break;
            }

            case TYPE_BIN_DATA:
            {
                storeDword(bfr, oneArg);        // first store length
                bfr += 4;

                uint8_t* pBinData = va_arg(args, uint8_t*); // get pointer to data
                memcpy(bfr, pBinData, oneArg);  // copy in the binary data
                bfr += oneArg;                  // advance by binary data size
                break;
            }

            default:            // this shouldn't happen
                break;
        }
    }

    va_end(args);               // end variable argument count access

    uint32_t argsBytes = bfr - bfrArgs; // how many bytes were stored

    //---------
    // do the actual function call
    CEXcall cc;
    cc.readNotWrite = DATA_DIR_WRITE;
    cc.cmd = FUNC_RAW_WRITE;        // ACSI command that will be used
    cc.extensionId = extensionId;   // ID of extension
    cc.functionId = sign->index;    // function id is index in the signature table
    cc.sectorCount = __bytesLenToSectorsLen(argsBytes);     // bytes count to sector count
    cc.arg1 = 0;
    cc.arg2 = 0;
    cc.buffer = bfrArgs;            // where the args are stored

    return __doTheCall(&cc);        // send this to CE
}

// Get response on last called function and stores it in the pResponse buffer.
uint8_t cexResponse(uint8_t extensionId, char* functionName, uint32_t expectRespLenBytes, uint8_t* pResponse)
{
    BinarySignatureForST* sign = __getSignatureByName(functionName);

    if(!sign) {     // signature by name not found? fail
        (void) Cconws("Function \33p");
        (void) Cconws(functionName);
        (void) Cconws("\33q not found!\r\n");
        return STATUS_NO_SUCH_FUNCTION;
    }

    CEXcall cc;
    cc.readNotWrite = DATA_DIR_READ;
    cc.cmd = FUNC_RAW_READ;                 // ACSI command that will be used
    cc.extensionId = extensionId;           // ID of extension
    cc.functionId = CEX_FUN_GET_RESPONSE;
    cc.sectorCount = __bytesLenToSectorsLen(expectRespLenBytes);      // bytes count to sector count
    cc.arg1 = sign->index;                  // function id placed on arg1 for status / response
    cc.arg2 = 0;
    cc.buffer = pResponse;          // where the data will be stored after this call

    return __doTheCall(&cc);        // send this to CE
}

// Use this function to describe exported function identified by functionName.
void cexDescribe(char* functionName)
{
    BinarySignatureForST* sign = __getSignatureByName(functionName);

    if(!sign) {     // function not found?
        (void) Cconws("Function name ");
        (void) Cconws(functionName);
        (void) Cconws(" not found in extension!\r\n");
        return;
    }

    (void) Cconws("Describing function \33p");
    (void) Cconws(functionName);
    (void) Cconws("\33q\r\n");

    (void) Cconws("Expected argument count: ");
    showInt(sign->argumentsCount, -1);
    (void) Cconws("\r\n");

    int i, position=0;
    for(i=0; i<sign->argumentsCount; i++) {                 // go through all the function arguments
        if(sign->argumentTypes[i] == TYPE_BIN_DATA) {       // if the type is binary data, insert additional size argument
            (void) Cconws("argument ");
            showInt(position, 2);
            (void) Cconws(": binary data size in bytes\r\n");
            position++;
        }

        (void) Cconws("argument ");     // dump argument like specified in signature
        showInt(position, 2);
        (void) Cconws(": ");
        (void) Cconws(__getArgumentTypeString(sign->argumentTypes[i]));     // show type as string
        (void) Cconws("\r\n");
        position++;
    }

    (void) Cconws("response: ");
    (void) Cconws(__getResponseTypeString(sign->returnValueType));
    (void) Cconws("\r\n");
}

// Calculate hash from supplied function name.
uint16_t __calcHash(char* name)
{
    uint16_t res = 0;
    int len = strlen(name);
    len = ((len & 1) == 1) ? (len + 1) : len;           // if odd length, make it even, otherwise just leave

    int i;
    for(i=0; i<len; i += 2) {
        uint16_t twoChars = getWord((uint8_t*) (name + i));
        res = res ^ twoChars;
    }

    return res;
}

// Find function signature by supplied function name. Returns pointer to signarure on success, or zero on failure.
BinarySignatureForST* __getSignatureByName(char* functionName)
{
    uint16_t hash = __calcHash(functionName);   // name to hash

    if(hash == signOpen.nameHash) {             // it's a CEX_FUN_OPEN?
        return &signOpen;
    }

    int i;
    for(i=0; i<MAX_EXPORTED_FUNCTIONS; i++) {
        if(signatures[i].nameHash == hash) {    // found matching hash? return pointer
            return &signatures[i];
        }
    }

    return 0;   // no matching hash, no pointer
}

// Validate function name, function type, arguments count. 
// Use DONT_CHECK_ARG_COUNT in argsCount if argument count check should be skipped.
// Returns signature on success, return 0 on failure and shows function description on console.
BinarySignatureForST* __validate(char* functionName, uint8_t functionType, uint8_t argsCount)
{
    // try to find function signature by name
    BinarySignatureForST* sign = __getSignatureByName(functionName);

    if(!sign) {     // signature by name not found? fail
        (void) Cconws("Function \33p");
        (void) Cconws(functionName);
        (void) Cconws("\33q not found!\r\n");
        return 0;
    }

    uint8_t argCountOk = 1;     // start with OK value, so we can possibly skip check if needed
    if(argsCount != DONT_CHECK_ARG_COUNT) {                 // if should check argument count also
        uint8_t binDataArgsCount = 0;

        int i;
        for(i=0; i<sign->argumentsCount; i++) {             // go through the argument types and count how many TYPE_BIN_DATA are in signature
            if(sign->argumentTypes[i] == TYPE_BIN_DATA) {
                binDataArgsCount++;
            }
        }

        // count OK if supplied and signature values match (additioanlly, each TYPE_BIN_DATA adds 1 argument (length), so add that to count)
        argCountOk = (argsCount == (sign->argumentsCount + binDataArgsCount));
    }

    if(sign->funcType != functionType || !argCountOk) { // function type mismatch or argument count mismatch
        cexDescribe(functionName);
        return 0;       // no pointer on failure
    }

    return sign;        // return signature pointer on success
}

// TYPE_* integer to string.
const char* __getArgumentTypeString(uint8_t argType)
{
    switch(argType) {
        case TYPE_NOT_PRESENT:  return "not present";
        case TYPE_UINT8:        return "uint8_t";
        case TYPE_UINT16:       return "uint16_t";
        case TYPE_UINT32:       return "uint32_t";
        case TYPE_INT8:         return "int8_t";
        case TYPE_INT16:        return "int16_t";
        case TYPE_INT32:        return "int32_t";
        case TYPE_CSTRING:      return "zero terminated string";
        case TYPE_PATH:         return "path";
        case TYPE_BIN_DATA:     return "binary data";
        default:                return "unknown";
    }
}

// RESP_* integer to string.
const char* __getResponseTypeString(uint8_t responseType)
{
    switch(responseType) {
        case RESP_TYPE_NONE:                return "none";
        case RESP_TYPE_STATUS:              return "status byte only";
        case RESP_TYPE_STATUS_BIN_DATA:     return "binary data and status";
        case RESP_TYPE_STATUS_PATH:         return "zero terminated string";
        default:                            return "unknown";
    }
}

// turn length in bytes to length in sectors
uint8_t __bytesLenToSectorsLen(uint32_t dataLenBytes)
{
    uint32_t sectors = dataLenBytes / 512;

    if((dataLenBytes % 512) != 0) {
        sectors++;
    }

    return sectors;
}

// Get pointer to buffer, starting on even address.
uint8_t* __getBuffer(void)
{
    static uint8_t bfr[513];                // slightly larger than one sector
    static uint8_t* pBfrEven = 0;           // pointer to buffer, starting on even address

    if(pBfrEven != 0) {                     // already got even value? return it
        return pBfrEven;
    }

    uint32_t toEven = (uint32_t) &bfr[0];   // get pointer to start

    if(toEven & 1) {                        // not even number? 
        toEven++;
    }

    pBfrEven = (uint8_t*) toEven;           // store pointer for later use
    return pBfrEven;
}

// Pass this call specified in cc to CE_DD driver, which will do the actual sending of data to CE.
uint8_t __doTheCall(CEXcall* cc)
{
    uint32_t res;
    // When this is a OPEN call, verify that the CE_DD is present and responsing. 
    // Don't do this check for other calls - for higher call speed.
    if(cc->functionId == CEX_FUN_OPEN) {
        res = Fopen("CEDD", TAG_CE);        // filename: "CEDD", mode: 'CE'

        if(res != RET_CEDD) {               // if return value is not 'CEDD', fail
            (void) Cconws("CE_DD not running or old version.\r\n");
            return STATUS_EXT_NOT_RUNNING;
        }
    }

    // fill the call string with special tag and pointer to structure
    uint8_t callStr[10];
    memcpy(callStr, "CEXT", 4);                 // 0-3: 'CEXT' - our special tag
    storeDword(callStr + 4, (uint32_t) cc);     // 4-7: pointer to CEXcall struct
    callStr[8] = 0;                             // 8: zero termination

    // do the actual call - for this the CE_DD must be running and responding on special Fopen call
    res = Fopen(callStr, TAG_CX);               // filename: call string, mode: 'CX'
    if(res != RET_CEXT) {                       // return value not 'CEXT'? fail
        return STATUS_EXT_NOT_RUNNING;
    }

    return cc->statusByte;      // return the status byte
}
