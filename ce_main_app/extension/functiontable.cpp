#include <stdint.h>
#include <string.h>
#include "functiontable.h"
#include "extensiondefs.h"

void FunctionTable::clear(void)
{
    for(int i=0; i<MAX_EXPORTED_FUNCTIONS; i++) {
        signatures[i].clear();
    }
}

int FunctionTable::getFunctionIndexByName(char* name)
{
    if(strcmp(name, "CEX_FUN_OPEN") == 0) {         // extension opened?
        return FAKE_INDEX_OPEN;
    }

    if(strcmp(name, "FAKE_INDEX_CLOSE") == 0) {     // extension closed?
        return FAKE_INDEX_OPEN;
    }

    for(int i=0; i<MAX_EXPORTED_FUNCTIONS; i++) {
        if(!signatures[i].used) {       // first not used signature means end of function table
            break;
        }

        if(signatures[i].nameMatching(name)) {  // if name matching, return this index
            return i;
        }
    }

    return -1;      // not found
}

// go through all the signatures and export them in format for ST
uint32_t FunctionTable::exportBinarySignatures(uint8_t* buffer)
{
    BinarySignatureForST* sign = (BinarySignatureForST*) buffer;

    for(int i=0; i<MAX_EXPORTED_FUNCTIONS; i++) {
        signatures[i].exportBinarySignature(i, sign);      // export signature at index i
        sign++;                                             // move to next signature place
    }

    return (MAX_EXPORTED_FUNCTIONS * sizeof(BinarySignatureForST)); // return size of stored data
}

void FunctionTable::storeReceivedSignatures(uint8_t* buffer, uint8_t funcCount)
{
    ReceivedSignature* sign = (ReceivedSignature*) buffer;

    // store all the signatures
    for(uint8_t i=0; i<funcCount; i++) {
        signatures[i].store(sign->name, sign->argumentsCount, sign->argumentTypes, sign->returnValueType);
        sign++;     // more to next signature by incrementing pointer
    }
}
