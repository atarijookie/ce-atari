#include <stdint.h>
#include <string.h>

#include "../debug.h"
#include "../utils.h"
#include "functionsignature.h"
#include "extensiondefs.h"

void FunctionSignature::clear(void)
{
    used = false;

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

void FunctionSignature::store(char* name, uint8_t argumentsCount, uint8_t* argumentTypes, uint8_t returnValueType)
{
    clear();            // clear everything to initial state

    used = true;        // mark signature as used
    setName(name);      // copy name, calc hash

    Debug::out(LOG_DEBUG, "FunctionSignature::store - function '%s', hash: %04x, argumentsCount: %d", name, nameHash, argumentsCount);

    // store arguments
    int storeArgsCount = MIN((int) argumentsCount, MAX_FUNCTION_ARGUMENTS);
    memcpy(this->argumentTypes, argumentTypes, storeArgsCount);

    this->returnValueType = returnValueType;    // store return value type
}

void FunctionSignature::exportBinarySignature(int index, BinarySignatureForST* sign)
{
    sign->index = (uint8_t) index + (CEX_FUN_CLOSE + 1);    // store index increased beyond the fixed functions (open, status, response, close)
    Utils::storeWord(sign->nameHash, nameHash);             // store name hash with endiannes independend method
    memcpy(sign->argumentTypes, argumentTypes, MAX_FUNCTION_ARGUMENTS);     // copy all the arguments
    sign->returnValueType = returnValueType;
}
