#ifndef FUNCTIONTABLE_H
#define FUNCTIONTABLE_H

#include <string>
#include <stdint.h>

#include "functionsignature.h"
#include "extensiondefs.h"

class FunctionTable
{
public:
    void clear(void);
    int getFunctionIndexByName(char* name);

    void storeReceivedSignatures(uint8_t* buffer, uint8_t funcCount);
    uint32_t exportBinarySignatures(uint8_t* buffer);

    FunctionSignature signatures[MAX_EXPORTED_FUNCTIONS];
};

#endif
