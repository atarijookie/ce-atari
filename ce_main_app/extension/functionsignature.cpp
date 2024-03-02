#include <stdint.h>
#include <string.h>

#include "../debug.h"
#include "../utils.h"
#include "functionsignature.h"
#include "extensiondefs.h"

void FunctionSignature::clear(void)
{
    used = false;

    funcType = FUNC_NONE;
    argumentsCount = 0;

    for(int i=0; i<MAX_FUNCTION_ARGUMENTS; i++) {
        argumentTypes[i] = TYPE_NOT_PRESENT;
    }

    setName("");
}

void FunctionSignature::setName(const char* name)
{
    memset(this->name, 0, sizeof(this->name));
    strncpy(this->name, name, sizeof(this->name) - 1);      // copy in the name

    nameHash = calcHash(this->name);    // calc and store name hash of potentially truncated name
}

void FunctionSignature::getName(char* nameBuffer, int maxLen)
{
    memset(nameBuffer, 0, maxLen);
    strncpy(nameBuffer, name, MIN((unsigned int) (maxLen - 1), sizeof(name) - 1));
}

bool FunctionSignature::nameMatching(char* inName)
{
    return strncmp(inName, name, sizeof(name) - 1) == 0;
}

uint16_t FunctionSignature::calcHash(char* name)
{
    uint16_t res = 0;
    int len = MIN(sizeof(this->name), strlen(name));
    len = ((len & 1) == 1) ? (len + 1) : len;           // if odd length, make it even, otherwise just leave

    for(int i=0; i<len; i += 2) {
        uint16_t twoChars = Utils::getWord((uint8_t*) (name + i));
        res = res ^ twoChars;
    }

    return res;
}

void FunctionSignature::dumpReceivedSignature(ReceivedSignature* sign)
{
    Debug::out(LOG_DEBUG, "ReceivedSignature - name: '%s', funcType: %d, argumentsCount: %d, argumentTypes: %d %d %d %d %d %d %d %d %d %d, returnValueType: %d",
        sign->name, sign->funcType, sign->argumentsCount, sign->argumentTypes[0], sign->argumentTypes[1], sign->argumentTypes[2], sign->argumentTypes[3],
        sign->argumentTypes[4], sign->argumentTypes[5], sign->argumentTypes[6], sign->argumentTypes[7], sign->argumentTypes[8], sign->argumentTypes[9],
        sign->returnValueType);
}

void FunctionSignature::dump(void)
{
    Debug::out(LOG_DEBUG, "stored signature - used: %d, name: '%s', nameHash: %04x, funcType: %d, argumentsCount: %d, argumentTypes: %d %d %d %d %d %d %d %d %d %d, returnValueType: %d",
        used, name, nameHash, funcType, argumentsCount, argumentTypes[0], argumentTypes[1], argumentTypes[2], argumentTypes[3],
        argumentTypes[4], argumentTypes[5], argumentTypes[6], argumentTypes[7], argumentTypes[8], argumentTypes[9],
        returnValueType);
}

void FunctionSignature::store(ReceivedSignature* sign)
{
    dumpReceivedSignature(sign);        // dump to logs what came in

    clear();                // clear everything to initial state
    used = true;            // mark signature as used
    setName(sign->name);    // copy name, calc hash

    funcType = sign->funcType;  // store type of function

    // store arguments
    argumentsCount = MIN((int) sign->argumentsCount, MAX_FUNCTION_ARGUMENTS);
    memcpy(argumentTypes, sign->argumentTypes, argumentsCount);
    returnValueType = sign->returnValueType;    // store return value type

    dump();                 // dump to logs what we've stored
}

void FunctionSignature::exportBinarySignature(int index, BinarySignatureForST* sign)
{
    sign->index = (uint8_t) index + (CEX_FUN_CLOSE + 1);    // store index increased beyond the fixed functions (open, status, response, close)
    Utils::storeWord(sign->nameHash, nameHash);             // store name hash with endiannes independend method
    sign->funcType = funcType;
    sign->argumentsCount = argumentsCount;
    memcpy(sign->argumentTypes, argumentTypes, MAX_FUNCTION_ARGUMENTS);     // copy all the arguments
    sign->returnValueType = returnValueType;
}

uint8_t FunctionSignature::getAcsiCmdForFuncType(void)
{
    /*
        Turn function call type into actual ACSI command which must be used to call this function.

        FUNC_NONE       -> 0
        FUNC_RAW_READ   -> CMD_CALL_RAW_READ
        FUNC_RAW_WRITE  -> CMD_CALL_RAW_WRITE
        FUNC_LONG_ARGS  -> CMD_CALL_LONG_WRITE_ARGS
    */
    const static uint8_t funcTypeToJustCmd[4] = {0, CMD_CALL_RAW_READ, CMD_CALL_RAW_WRITE, CMD_CALL_LONG_WRITE_ARGS};

    if(funcType >= sizeof(funcTypeToJustCmd)) { // index would be too big?
        Debug::out(LOG_WARNING, "FunctionSignature::getAcsiCmdForFuncType - funcType %d is out of bounds!", funcType);
        return 0;
    }

    return funcTypeToJustCmd[funcType];         // return which ACSI command should be used
}
