#ifndef FUNCTIONSIGNATURE_H
#define FUNCTIONSIGNATURE_H

#include <string>
#include <stdint.h>
#include "extensiondefs.h"

class FunctionSignature
{
public:
    void clear(void);
    void setName(const char* name);
    void getName(char* nameBuffer, int maxLen);
    bool nameMatching(char* inName);
    uint16_t calcHash(char* name);
    void store(char* name, uint8_t argumentsCount, uint8_t* argumentTypes, uint8_t returnValueType);
    void exportBinarySignature(int index, BinarySignatureForST* sign);

    bool used;
    char name[MAX_FUNCTION_NAME_LEN];
    uint16_t nameHash;
    uint8_t argumentTypes[MAX_FUNCTION_ARGUMENTS];
    uint8_t returnValueType;
};

#endif
